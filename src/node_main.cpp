#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

#include "dht22_sensor.h"
#include "packet_protocol.h"
#include "project_config.h"
#include "soil_moisture.h"

RTC_DATA_ATTR uint32_t rtcPacketId = 0;
RTC_DATA_ATTR uint32_t rtcSleepMinutes = Config::DefaultSleepMinutes;

static Dht22Sensor dht22;
static SoilMoistureSensor soilSensor;

static void setPump(bool enabled)
{
    digitalWrite(Config::PumpPin, enabled == Config::PumpActiveHigh ? HIGH : LOW);
}

static bool initLoRa()
{
    for (uint8_t attempt = 1; attempt <= 3; attempt++)
    {
        LoRa.end();
        SPI.end();
        delay(50);

        pinMode(Config::LoraSs, OUTPUT);
        digitalWrite(Config::LoraSs, HIGH);
        pinMode(Config::LoraRst, OUTPUT);
        digitalWrite(Config::LoraRst, LOW);
        delay(50);
        digitalWrite(Config::LoraRst, HIGH);
        delay(200);

        SPI.begin(Config::LoraSck, Config::LoraMiso, Config::LoraMosi, Config::LoraSs);
        LoRa.setPins(Config::LoraSs, Config::LoraRst, Config::LoraDio0);
        if (LoRa.begin(Config::LoraFrequency))
        {
            LoRa.setSpreadingFactor(Config::LoraSpreadingFactor);
            LoRa.setSignalBandwidth(Config::LoraSignalBandwidth);
            LoRa.setCodingRate4(Config::LoraCodingRate);
            LoRa.setTxPower(Config::LoraTxPowerDbm);
            LoRa.enableCrc();
            return true;
        }

        Serial.printf("LoRa init retry %u failed\n", attempt);
    }
    return false;
}

static float readBatteryVoltage()
{
    analogReadResolution(12);
    analogSetPinAttenuation(Config::BatteryAdcPin, ADC_11db);
    delay(10);

    uint32_t rawSum = 0;
    uint32_t milliVoltSum = 0;
    constexpr uint8_t sampleCount = 16;
    for (uint8_t i = 0; i < sampleCount; i++)
    {
        rawSum += analogRead(Config::BatteryAdcPin);
        milliVoltSum += analogReadMilliVolts(Config::BatteryAdcPin);
        delay(2);
    }

    const float adcVoltage = (milliVoltSum / float(sampleCount)) / 1000.0f;
    return adcVoltage * Config::BatteryDividerRatio;
}

static uint32_t computeAdaptiveSleepMinutes(const TelemetryPacket &telemetry)
{
    if (telemetry.errorFlag != 0)
    {
        return 30;
    }
    if (telemetry.batteryV < 3.3f)
    {
        return 90;
    }
    if (telemetry.batteryV < 3.5f)
    {
        return 60;
    }
    if (telemetry.soilStatus == "URGENT_WATERING")
    {
        return 5;
    }
    if (telemetry.soilStatus == "NEED_WATERING")
    {
        return 10;
    }
    if (telemetry.soilStatus == "LIGHT_DRY")
    {
        return 20;
    }
    if (telemetry.soilStatus == "OVER_MOISTURE")
    {
        return 60;
    }
    return 30;
}

static uint32_t computeAdaptiveSleepSeconds(const TelemetryPacket &telemetry)
{
    if (Config::EnableFastUrgentSleepTest && telemetry.soilStatus == "URGENT_WATERING" &&
        telemetry.errorFlag == 0)
    {
        return Config::FastUrgentSleepSeconds;
    }

    return computeAdaptiveSleepMinutes(telemetry) * 60UL;
}

static void executeCommand(const AckPacket &ack)
{
    if (!ack.ok)
    {
        return;
    }

    switch (ack.command)
    {
    case CommandType::SetSleepDuration:
        if (ack.parameter >= 5 && ack.parameter <= 90)
        {
            rtcSleepMinutes = ack.parameter;
            Serial.printf("Command SET_SLEEP_DURATION: %lu min\n", rtcSleepMinutes);
        }
        break;
    case CommandType::SleepNow:
        Serial.println("Command SLEEP_NOW");
        break;
    case CommandType::StartPump:
    {
        const int pumpSeconds = constrain(ack.parameter, 0, int(Config::MaxPumpSeconds));
        Serial.printf("Command START_PUMP: %d s\n", pumpSeconds);
        if (pumpSeconds > 0)
        {
            setPump(true);
            delay(uint32_t(pumpSeconds) * 1000UL);
            setPump(false);
            Serial.println("Pump OFF");
        }
        break;
    }
    case CommandType::StartOta:
        Serial.println("Command START_OTA received; OTA-over-LoRa handler is reserved for firmware chunk flow");
        break;
    default:
        break;
    }
}

static bool waitForAck(uint32_t packetId)
{
    const uint32_t start = millis();
    while (millis() - start < Config::AckTimeoutMs)
    {
        const int packetSize = LoRa.parsePacket();
        if (packetSize <= 0)
        {
            delay(10);
            continue;
        }

        String line;
        while (LoRa.available())
        {
            line += char(LoRa.read());
        }

        AckPacket ack;
        if (!decodeAck(line, ack))
        {
            Serial.println("ACK invalid CRC/header");
            Serial.print("ACK raw: ");
            Serial.println(line);
            continue;
        }
        if (ack.nodeId != Config::NodeId || ack.packetId != packetId)
        {
            Serial.println("ACK ignored: Node_ID/Packet_ID mismatch");
            continue;
        }

        Serial.print("ACK received: ");
        Serial.println(line);
        executeCommand(ack);
        return ack.ok;
    }
    return false;
}

static bool sendTelemetryWithRetry(const TelemetryPacket &telemetry)
{
    const String payload = encodeTelemetry(telemetry);
    for (uint8_t retry = 0; retry < Config::MaxRetry; retry++)
    {
        Serial.printf("LoRa TX try %u: %s\n", retry + 1, payload.c_str());
        LoRa.beginPacket();
        LoRa.print(payload);
        LoRa.endPacket(true);
        delay(250);
        LoRa.receive();

        if (waitForAck(telemetry.packetId))
        {
            return true;
        }
        delay(500);
    }
    return false;
}

static void enterSleepSeconds(uint32_t sleepSeconds)
{
    sleepSeconds = constrain(sleepSeconds, 5UL, 90UL * 60UL);
    rtcSleepMinutes = max(1UL, sleepSeconds / 60UL);
    LoRa.sleep();
    LoRa.end();
    SPI.end();
    digitalWrite(Config::SensorPowerPin, LOW);

    if (sleepSeconds < 60)
    {
        Serial.printf("Sleep duration: %lu sec\n", sleepSeconds);
    }
    else
    {
        Serial.printf("Sleep duration: %lu min\n", sleepSeconds / 60UL);
    }
    Serial.flush();

    if (Config::EnableDeepSleep)
    {
        esp_sleep_enable_timer_wakeup(uint64_t(sleepSeconds) * 1000000ULL);
        esp_deep_sleep_start();
    }
}

static void enterSleep(uint32_t sleepMinutes)
{
    enterSleepSeconds(sleepMinutes * 60UL);
}

void setup()
{
    Serial.begin(Config::SerialBaud);
    delay(1000);

    pinMode(Config::SensorPowerPin, OUTPUT);
    digitalWrite(Config::SensorPowerPin, HIGH);
    pinMode(Config::PumpPin, OUTPUT);
    setPump(false);
    delay(2000);

    Serial.println("===== EE4552 NODE FIRMWARE =====");
    Serial.printf("Wake cause: %d\n", int(esp_sleep_get_wakeup_cause()));

    dht22.begin(Config::DhtPin, Config::TempOffsetC, Config::HumidityOffsetRh);
    soilSensor.begin(Config::SoilAdcPin, Config::SoilAdcDry, Config::SoilAdcWet,
                     Config::SoilAdcMin, Config::SoilAdcMax);

    if (!initLoRa())
    {
        Serial.println("LoRa init failed");
        enterSleep(5);
        return;
    }
    Serial.println("LoRa init OK");

    const DhtReading dht = dht22.read();
    const SoilReading soil = soilSensor.read();
    const float batteryV = readBatteryVoltage();

    TelemetryPacket telemetry;
    telemetry.nodeId = Config::NodeId;
    telemetry.packetId = ++rtcPacketId;
    telemetry.temperatureC = dht.temperatureC;
    telemetry.humidityRh = dht.humidityRh;
    telemetry.soilMoistureVol = soil.moistureVol;
    telemetry.batteryV = batteryV;
    telemetry.adcFiltered = soil.adcFiltered;
    telemetry.errorFlag = dht.errorFlag || soil.errorFlag;
    telemetry.soilStatus = SoilMoistureSensor::statusText(soil.soilStatus);

    Serial.printf("DHT: T=%.1f C H=%.1f %%RH ERR=%u\n", telemetry.temperatureC,
                  telemetry.humidityRh, dht.errorFlag);
    Serial.printf("SOIL: ADC=%d H=%.1f %%Vol status=%s ERR=%u\n", telemetry.adcFiltered,
                  telemetry.soilMoistureVol, telemetry.soilStatus.c_str(), soil.errorFlag);
    Serial.printf("Battery: %.2f V\n", telemetry.batteryV);

    const bool delivered = sendTelemetryWithRetry(telemetry);
    if (!delivered)
    {
        Serial.println("No ACK after MAX_RETRY");
    }

    enterSleepSeconds(computeAdaptiveSleepSeconds(telemetry));
}

void loop()
{
    if (!Config::EnableDeepSleep)
    {
        delay(uint32_t(rtcSleepMinutes) * 60UL * 1000UL);
        ESP.restart();
    }
}
