#include "gateway_lora.h"

#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

#include "gateway_cloud.h"
#include "gateway_logic.h"
#include "gateway_state.h"
#include "packet_protocol.h"
#include "project_config.h"

static void sendAckFor(const TelemetryPacket &packet);

bool initLoRa()
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
        LoRa.end();
        SPI.end();
        return false;
    }

    LoRa.setSpreadingFactor(Config::LoraSpreadingFactor);
    LoRa.setSignalBandwidth(Config::LoraSignalBandwidth);
    LoRa.setCodingRate4(Config::LoraCodingRate);
    LoRa.setTxPower(Config::LoraTxPowerDbm);
    LoRa.enableCrc();
    LoRa.receive();
    return true;
}

static bool waitForOtaStatus(uint8_t nodeId, uint32_t otaId, uint16_t chunkIndex,
                             const String &expectedStatus, OtaStatusPacket &status)
{
    const uint32_t start = millis();
    while (millis() - start < 7000)
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

        if (!decodeOtaStatus(line, status))
        {
            TelemetryPacket telemetry;
            if (chunkIndex == 0 && decodeTelemetry(line, telemetry) && telemetry.nodeId == nodeId)
            {
                Serial.println("OTA wait received DATA; resending START_OTA ACK");
                sendAckFor(telemetry);
                continue;
            }
            Serial.print("OTA status invalid: ");
            Serial.println(line);
            continue;
        }
        if (status.nodeId != nodeId || status.otaId != otaId)
        {
            Serial.println("OTA status ignored: node/ota mismatch");
            continue;
        }
        if (status.chunkIndex != chunkIndex && expectedStatus != "OTA_SUCCESS")
        {
            Serial.printf("OTA status ignored: chunk mismatch idx=%u expected=%u status=%s\n",
                          status.chunkIndex, chunkIndex, status.status.c_str());
            continue;
        }
        if (expectedStatus.length() > 0 && status.status != expectedStatus)
        {
            Serial.printf("OTA status unexpected: %s\n", status.status.c_str());
            return false;
        }
        return status.ok;
    }
    return false;
}

static bool sendOtaChunk(uint8_t nodeId, uint32_t otaId, uint16_t index,
                         uint16_t total, const String &data)
{
    constexpr uint8_t maxAttempts = 3;
    for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++)
    {
        OtaChunkPacket chunk;
        chunk.nodeId = nodeId;
        chunk.otaId = otaId;
        chunk.chunkIndex = index;
        chunk.totalChunks = total;
        chunk.payloadData = data;
        chunk.dataCrc = crc16Ccitt(data);

        const String payload = encodeOtaChunk(chunk);
        Serial.printf("OTA CHUNK TX idx=%u try=%u: %s\n", index, attempt, payload.c_str());
        LoRa.beginPacket();
        LoRa.print(payload);
        LoRa.endPacket();
        LoRa.receive();
        delay(150);

        OtaStatusPacket status;
        if (waitForOtaStatus(nodeId, otaId, index, "ACK", status))
        {
            return true;
        }
        Serial.printf("OTA chunk retry idx=%u next_try=%u\n", index, attempt + 1);
    }
    return false;
}

static void runSimulatedLoraOtaTransfer(uint8_t nodeId, uint32_t otaId)
{
    if (otaSessionActive)
    {
        return;
    }
    otaSessionActive = true;

    static const char *chunks[] = {
        "FW_SIM_001_BOOT_HDR",
        "FW_SIM_002_PARTITION",
        "FW_SIM_003_APP_TEXT",
        "FW_SIM_004_APP_DATA",
        "FW_SIM_005_DRIVER",
        "FW_SIM_006_SENSOR",
        "FW_SIM_007_LORA",
        "FW_SIM_008_CONFIG",
        "FW_SIM_009_CHECKSUM",
        "FW_SIM_010_END"};

    Serial.printf("OTA session start node=%u ota_id=%lu\n", nodeId, otaId);
    LoRa.receive();
    delay(500);

    constexpr uint16_t totalChunks = sizeof(chunks) / sizeof(chunks[0]);
    for (uint16_t i = 0; i < totalChunks; i++)
    {
        if (!sendOtaChunk(nodeId, otaId, i + 1, totalChunks, chunks[i]))
        {
            Serial.printf("OTA session failed at chunk %u\n", i + 1);
            otaSessionActive = false;
            return;
        }
    }

    OtaStatusPacket done;
    if (waitForOtaStatus(nodeId, otaId, totalChunks, "OTA_SUCCESS", done))
    {
        Serial.printf("OTA session success node=%u ota_id=%lu chunks=%u\n", nodeId, otaId, totalChunks);
    }
    else
    {
        Serial.println("OTA session finished chunks but final OTA_SUCCESS not received");
    }
    otaSessionActive = false;
}

static void sendAckFor(const TelemetryPacket &packet)
{
    if (!loraReady)
    {
        return;
    }

    AckPacket ack;
    ack.nodeId = packet.nodeId;
    ack.packetId = packet.packetId;
    ack.ok = packet.errorFlag == 0;
    ack.status = ack.ok ? "OK" : "SENSOR_ERROR";

    CommandType queuedCommand = CommandType::None;
    int queuedParameter = 0;
    portENTER_CRITICAL(&pendingCommandMux);
    queuedCommand = pendingCommands[packet.nodeId].command;
    queuedParameter = pendingCommands[packet.nodeId].parameter;
    if (queuedCommand != CommandType::None)
    {
        if (pendingCommandRepeats[packet.nodeId] > 0)
        {
            pendingCommandRepeats[packet.nodeId]--;
        }
        if (pendingCommandRepeats[packet.nodeId] == 0)
        {
            pendingCommands[packet.nodeId].command = CommandType::None;
            pendingCommands[packet.nodeId].parameter = 0;
            pendingCommands[packet.nodeId].status = "SENT";
        }
    }
    portEXIT_CRITICAL(&pendingCommandMux);

    if (queuedCommand != CommandType::None)
    {
        ack.command = queuedCommand;
        ack.parameter = queuedParameter;
        ack.status = "COMMAND";
    }
    else if (Config::EnablePumpHardware)
    {
        const int pumpTime = computePumpTime(packet);
        if (pumpTime > 0)
        {
            ack.command = CommandType::StartPump;
            ack.parameter = pumpTime;
        }
    }

    const String payload = encodeAck(ack);
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
    LoRa.receive();
    delay(100);
    Serial.print("ACK TX: ");
    Serial.println(payload);

    if (queuedCommand == CommandType::StartOta)
    {
        otaReadyWaitActive = true;
        otaReadyWaitNodeId = packet.nodeId;
        otaReadyWaitId = uint32_t(ack.parameter);
        otaReadyWaitDeadlineMs = millis() + 30000UL;
        Serial.printf("OTA waiting for OTA_READY node=%u ota_id=%lu\n", otaReadyWaitNodeId, otaReadyWaitId);
    }
}

void processLoRa()
{
    if (!loraReady)
    {
        return;
    }

    const int packetSize = LoRa.parsePacket();
    if (packetSize <= 0)
    {
        return;
    }

    String line;
    while (LoRa.available())
    {
        line += char(LoRa.read());
    }

    OtaStatusPacket otaStatus;
    if (decodeOtaStatus(line, otaStatus))
    {
        Serial.printf("OTA STATUS RX node=%u ota_id=%lu idx=%u ok=%u status=%s\n",
                      otaStatus.nodeId, otaStatus.otaId, otaStatus.chunkIndex,
                      otaStatus.ok ? 1 : 0, otaStatus.status.c_str());
        if (otaReadyWaitActive && otaStatus.nodeId == otaReadyWaitNodeId &&
            otaStatus.otaId == otaReadyWaitId && otaStatus.status == "OTA_READY" && otaStatus.ok)
        {
            otaReadyWaitActive = false;
            runSimulatedLoraOtaTransfer(otaStatus.nodeId, otaStatus.otaId);
        }
        return;
    }

    TelemetryPacket packet;
    if (!decodeTelemetry(line, packet))
    {
        Serial.print("Invalid RF packet: ");
        Serial.println(line);
        return;
    }

    if (lastPacketIdByNode[packet.nodeId] == packet.packetId)
    {
        Serial.printf("Duplicate packet ignored: node=%u pid=%lu\n", packet.nodeId, packet.packetId);
        sendAckFor(packet);
        return;
    }
    lastPacketIdByNode[packet.nodeId] = packet.packetId;

    const int rssi = LoRa.packetRssi();
    const float snr = LoRa.packetSnr();
    storeTelemetry(packet, rssi, snr);

    Serial.printf("DATA RX node=%u pid=%lu T=%.1f H=%.1f Soil=%.0f ADC=%d V=%.2f RSSI=%d SNR=%.1f Alert=%s PumpTime=%d\n",
                  packet.nodeId, packet.packetId, packet.temperatureC, packet.humidityRh,
                  packet.soilMoistureVol, packet.adcFiltered, packet.batteryV, rssi, snr,
                  alertLevel(packet).c_str(), computePumpTime(packet));

    uploadThingsBoard(packet, rssi);
    sendAckFor(packet);
}

void processSerialCommand()
{
    if (!Serial.available())
    {
        return;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    line.toUpperCase();

    const int firstSpace = line.indexOf(' ');
    const int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0)
    {
        Serial.println("Serial command format: NODE_ID CMD PARAM");
        return;
    }

    const uint8_t nodeId = uint8_t(line.substring(0, firstSpace).toInt());
    const String cmdText = secondSpace < 0 ? line.substring(firstSpace + 1)
                                          : line.substring(firstSpace + 1, secondSpace);
    const int parameter = secondSpace < 0 ? 0 : line.substring(secondSpace + 1).toInt();
    const CommandType command = commandFromText(cmdText);
    if (nodeId == 0 || command == CommandType::None)
    {
        Serial.println("Invalid serial command. Example: 1 START_PUMP 5");
        return;
    }

    queueGatewayCommand(nodeId, command, parameter);
    Serial.printf("Queued command for node %u: %s %d\n", nodeId, cmdText.c_str(), parameter);
}
