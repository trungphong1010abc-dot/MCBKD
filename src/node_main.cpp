#include <Arduino.h>
#include <LoRa.h>
#include <Preferences.h>
#include <SPI.h>

#include "dht22_sensor.h"
#include "packet_protocol.h"
#include "project_config.h"
#include "soil_moisture.h"

RTC_DATA_ATTR uint32_t rtcPacketId = 0;
RTC_DATA_ATTR uint32_t rtcSleepMinutes = Config::DefaultSleepMinutes;

static Dht22Sensor dht22;
static SoilMoistureSensor soilSensor;
static Preferences preferences;

struct NodeRuntimeConfig
{
    int soilThresholdVol = 20;
    uint32_t sleepMinutes = Config::DefaultSleepMinutes;
    int filterMode = 0;  // 0=AVERAGE, 1=MEDIAN.
    int pumpSeconds = 5;
    int controlMode = 1; // 0=MANUAL, 1=AUTO.
    int dutyCycleMode = 1; // 0=FIXED, 1=ADAPTIVE.
};

static NodeRuntimeConfig runtimeConfig;

static void loadRuntimeConfig()
{
    preferences.begin("node_cfg", true);
    runtimeConfig.soilThresholdVol = preferences.getInt("soil_th", runtimeConfig.soilThresholdVol);
    runtimeConfig.sleepMinutes = preferences.getUInt("sleep_min", runtimeConfig.sleepMinutes);
    runtimeConfig.filterMode = preferences.getInt("filter", runtimeConfig.filterMode);
    runtimeConfig.pumpSeconds = preferences.getInt("pump_s", runtimeConfig.pumpSeconds);
    runtimeConfig.controlMode = preferences.getInt("ctrl", runtimeConfig.controlMode);
    runtimeConfig.dutyCycleMode = preferences.getInt("duty", runtimeConfig.dutyCycleMode);
    preferences.end();
    rtcSleepMinutes = runtimeConfig.sleepMinutes;
}

static void saveRuntimeConfig()
{
    preferences.begin("node_cfg", false);
    preferences.putInt("soil_th", runtimeConfig.soilThresholdVol);
    preferences.putUInt("sleep_min", runtimeConfig.sleepMinutes);
    preferences.putInt("filter", runtimeConfig.filterMode);
    preferences.putInt("pump_s", runtimeConfig.pumpSeconds);
    preferences.putInt("ctrl", runtimeConfig.controlMode);
    preferences.putInt("duty", runtimeConfig.dutyCycleMode);
    preferences.end();
}

static void setPump(bool enabled)
{
    digitalWrite(Config::PumpPin, enabled == Config::PumpActiveHigh ? HIGH : LOW);
}

static bool initLoRa()
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
    if (!LoRa.begin(Config::LoraFrequency))
    {
        return false;
    }

    LoRa.setSpreadingFactor(Config::LoraSpreadingFactor);
    LoRa.setSignalBandwidth(Config::LoraSignalBandwidth);
    LoRa.setCodingRate4(Config::LoraCodingRate);
    LoRa.setTxPower(Config::LoraTxPowerDbm);
    LoRa.enableCrc();
    return true;
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

    if (runtimeConfig.dutyCycleMode == 0)
    {
        return runtimeConfig.sleepMinutes * 60UL;
    }
    return computeAdaptiveSleepMinutes(telemetry) * 60UL;
}

static void sendOtaStatus(uint32_t otaId, uint16_t chunkIndex, bool ok, const String &status)
{
    OtaStatusPacket packet;
    packet.nodeId = Config::NodeId;
    packet.otaId = otaId;
    packet.chunkIndex = chunkIndex;
    packet.ok = ok;
    packet.status = status;

    const String payload = encodeOtaStatus(packet);
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
    LoRa.receive();
    delay(100);
    Serial.print("OTA STATUS TX: ");
    Serial.println(payload);
}

static bool waitForOtaChunk(uint32_t otaId, uint16_t expectedIndex, OtaChunkPacket &chunk,
                            bool advertiseReady)
{
    const uint32_t start = millis();
    uint32_t lastReadyMs = 0;
    while (millis() - start < 12000)
    {
        if (advertiseReady && millis() - lastReadyMs >= 1200)
        {
            lastReadyMs = millis();
            sendOtaStatus(otaId, 0, true, "OTA_READY");
        }

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

        if (!decodeOtaChunk(line, chunk))
        {
            Serial.print("OTA chunk invalid: ");
            Serial.println(line);
            sendOtaStatus(otaId, expectedIndex, false, "NACK_BAD_PACKET");
            continue;
        }
        if (chunk.nodeId != Config::NodeId || chunk.otaId != otaId || chunk.chunkIndex != expectedIndex)
        {
            Serial.println("OTA chunk ignored: node/ota/index mismatch");
            continue;
        }
        return true;
    }
    return false;
}

static void runSimulatedLoraOta(uint32_t otaId)
{
    if (otaId == 0)
    {
        otaId = rtcPacketId;
    }

    Serial.printf("START_OTA session ota_id=%lu\n", otaId);
    Serial.println("OTA simulation only; firmware flash is not modified");

    uint16_t expectedTotal = 0;
    uint16_t receivedChunks = 0;
    uint16_t firmwareCrc = 0xFFFF;

    while (true)
    {
        OtaChunkPacket chunk;
        const bool advertiseReady = receivedChunks == 0;
        if (!waitForOtaChunk(otaId, receivedChunks + 1, chunk, advertiseReady))
        {
            sendOtaStatus(otaId, receivedChunks + 1, false, "OTA_FAILED_TIMEOUT");
            return;
        }

        const uint16_t actualDataCrc = crc16Ccitt(chunk.payloadData);
        if (actualDataCrc != chunk.dataCrc)
        {
            Serial.printf("OTA chunk CRC fail idx=%u expected=%04X actual=%04X\n",
                          chunk.chunkIndex, chunk.dataCrc, actualDataCrc);
            sendOtaStatus(otaId, chunk.chunkIndex, false, "NACK_CRC");
            continue;
        }

        if (expectedTotal == 0)
        {
            expectedTotal = chunk.totalChunks;
        }
        firmwareCrc = crc16Ccitt(reinterpret_cast<const uint8_t *>(chunk.payloadData.c_str()),
                                 chunk.payloadData.length()) ^ firmwareCrc;
        receivedChunks++;
        sendOtaStatus(otaId, chunk.chunkIndex, true, "ACK");

        if (receivedChunks >= expectedTotal)
        {
            Serial.printf("OTA simulated firmware CRC=%04X chunks=%u\n", firmwareCrc, receivedChunks);
            delay(300);
            for (uint8_t i = 0; i < 3; i++)
            {
                sendOtaStatus(otaId, receivedChunks, true, "OTA_SUCCESS");
                delay(300);
            }
            return;
        }
    }
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
            runtimeConfig.sleepMinutes = ack.parameter;
            saveRuntimeConfig();
            Serial.printf("Command SET_SLEEP_DURATION: %lu min\n", rtcSleepMinutes);
        }
        break;
    case CommandType::SetThreshold:
        runtimeConfig.soilThresholdVol = constrain(ack.parameter, 0, 100);
        saveRuntimeConfig();
        Serial.printf("Command SET_THRESHOLD: soil_min=%d %%Vol\n", runtimeConfig.soilThresholdVol);
        break;
    case CommandType::SetFilterMode:
        runtimeConfig.filterMode = constrain(ack.parameter, 0, 1);
        saveRuntimeConfig();
        Serial.printf("Command SET_FILTER_MODE: %s\n", runtimeConfig.filterMode == 1 ? "MEDIAN" : "AVERAGE");
        break;
    case CommandType::SetPumpTime:
        runtimeConfig.pumpSeconds = constrain(ack.parameter, 0, int(Config::MaxPumpSeconds));
        saveRuntimeConfig();
        Serial.printf("Command SET_PUMP_TIME: %d s\n", runtimeConfig.pumpSeconds);
        break;
    case CommandType::SetControlMode:
        runtimeConfig.controlMode = constrain(ack.parameter, 0, 1);
        saveRuntimeConfig();
        Serial.printf("Command SET_CONTROL_MODE: %s\n", runtimeConfig.controlMode == 1 ? "AUTO" : "MANUAL");
        break;
    case CommandType::SetDutyCycle:
        runtimeConfig.dutyCycleMode = constrain(ack.parameter, 0, 1);
        saveRuntimeConfig();
        Serial.printf("Command SET_DUTY_CYCLE: %s\n", runtimeConfig.dutyCycleMode == 1 ? "ADAPTIVE" : "FIXED");
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
        runSimulatedLoraOta(uint32_t(ack.parameter));
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

static bool sendTelemetry(const TelemetryPacket &telemetry)
{
    const String payload = encodeTelemetry(telemetry);
    Serial.print("LoRa TX: ");
    Serial.println(payload);

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket(true);
    delay(250);
    LoRa.receive();

    return waitForAck(telemetry.packetId);
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
    loadRuntimeConfig();
    Serial.printf("Config: soil_threshold=%d sleep=%lu filter=%d pump=%d control=%d duty=%d\n",
                  runtimeConfig.soilThresholdVol, runtimeConfig.sleepMinutes,
                  runtimeConfig.filterMode, runtimeConfig.pumpSeconds,
                  runtimeConfig.controlMode, runtimeConfig.dutyCycleMode);

    dht22.begin(Config::DhtPin, Config::TempOffsetC, Config::HumidityOffsetRh);
    soilSensor.begin(Config::SoilAdcPin, Config::SoilAdcDry, Config::SoilAdcWet,
                     Config::SoilAdcMin, Config::SoilAdcMax);

    if (!initLoRa())
    {
        const uint32_t failureSleepMinutes = runtimeConfig.sleepMinutes > 0
                                                 ? runtimeConfig.sleepMinutes
                                                 : Config::DefaultSleepMinutes;
        Serial.printf("LoRa init failed; sleeping %lu min\n", failureSleepMinutes);
        enterSleep(failureSleepMinutes);
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
    telemetry.configSleepMinutes = runtimeConfig.sleepMinutes;
    telemetry.configSoilThresholdVol = runtimeConfig.soilThresholdVol;
    telemetry.configFilterMode = runtimeConfig.filterMode;
    telemetry.configPumpSeconds = runtimeConfig.pumpSeconds;
    telemetry.configControlMode = runtimeConfig.controlMode;
    telemetry.configDutyCycleMode = runtimeConfig.dutyCycleMode;

    Serial.printf("DHT: T=%.1f C H=%.1f %%RH ERR=%u\n", telemetry.temperatureC,
                  telemetry.humidityRh, dht.errorFlag);
    Serial.printf("SOIL: ADC=%d H=%.1f %%Vol status=%s ERR=%u\n", telemetry.adcFiltered,
                  telemetry.soilMoistureVol, telemetry.soilStatus.c_str(), soil.errorFlag);
    Serial.printf("Battery: %.2f V\n", telemetry.batteryV);

    const bool delivered = sendTelemetry(telemetry);
    if (!delivered)
    {
        Serial.println("No ACK; TX failed");
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
