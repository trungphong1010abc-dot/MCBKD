#pragma once

#include <Arduino.h>

enum class CommandType : uint8_t
{
    None = 0,
    SetSleepDuration = 1,
    SetThreshold = 2,
    SleepNow = 4,
    StartPump = 5,
    SetFilterMode = 7,
    SetPumpTime = 8,
    SetControlMode = 9,
    SetDutyCycle = 10
};

enum class PacketKind : uint8_t
{
    Telemetry = 1,
    Ack = 2,
    Command = 3
};

struct TelemetryPacket
{
    uint8_t nodeId = 0;
    uint32_t packetId = 0;
    float temperatureC = NAN;
    float humidityRh = NAN;
    float soilMoistureVol = NAN;
    int adcFiltered = 0;
    uint8_t errorFlag = 0;
    String soilStatus;
    uint32_t configSleepMinutes = 0;
    int configSoilThresholdVol = 0;
    int configFilterMode = 0;
    int configPumpSeconds = 0;
    int configControlMode = 0;
    int configDutyCycleMode = 0;
};

struct AckPacket
{
    uint8_t nodeId = 0;
    uint32_t packetId = 0;
    bool ok = false;
    CommandType command = CommandType::None;
    int parameter = 0;
    String status;
};

uint16_t crc16Ccitt(const uint8_t *data, size_t length);
uint16_t crc16Ccitt(const String &text);

String commandToText(CommandType command);
CommandType commandFromText(const String &text);

String encodeTelemetry(const TelemetryPacket &packet);
String encodeAck(const AckPacket &packet);
bool decodeTelemetry(const String &line, TelemetryPacket &packet);
bool decodeAck(const String &line, AckPacket &packet);
bool hasValidCrc(const String &line);
String getField(const String &line, const String &key);
