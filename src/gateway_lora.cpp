#include "gateway_lora.h"

#include <Arduino.h>
#include <LoRa.h>
#include <SPI.h>

#include "gateway_cloud.h"
#include "gateway_logic.h"
#include "gateway_state.h"
#include "packet_protocol.h"
#include "project_config.h"

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

    Serial.printf("DATA RX node=%u pid=%lu T=%.1f H=%.1f Soil=%.0f ADC=%d RSSI=%d SNR=%.1f Alert=%s PumpTime=%d\n",
                  packet.nodeId, packet.packetId, packet.temperatureC, packet.humidityRh,
                  packet.soilMoistureVol, packet.adcFiltered, rssi, snr,
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
