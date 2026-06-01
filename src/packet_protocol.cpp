#include "packet_protocol.h"

static String withoutCrc(const String &line)
{
    const int crcIndex = line.lastIndexOf(",CRC=");
    if (crcIndex < 0)
    {
        return line;
    }
    return line.substring(0, crcIndex);
}

uint16_t crc16Ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= uint16_t(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

uint16_t crc16Ccitt(const String &text)
{
    return crc16Ccitt(reinterpret_cast<const uint8_t *>(text.c_str()), text.length());
}

String commandToText(CommandType command)
{
    switch (command)
    {
    case CommandType::SetSleepDuration:
        return "SET_SLEEP_DURATION";
    case CommandType::SetThreshold:
        return "SET_THRESHOLD";
    case CommandType::StartOta:
        return "START_OTA";
    case CommandType::SleepNow:
        return "SLEEP_NOW";
    case CommandType::StartPump:
        return "START_PUMP";
    case CommandType::ResendChunk:
        return "RESEND_CHUNK";
    case CommandType::SetFilterMode:
        return "SET_FILTER_MODE";
    case CommandType::SetPumpTime:
        return "SET_PUMP_TIME";
    case CommandType::SetControlMode:
        return "SET_CONTROL_MODE";
    case CommandType::SetDutyCycle:
        return "SET_DUTY_CYCLE";
    default:
        return "NONE";
    }
}

CommandType commandFromText(const String &text)
{
    if (text == "SET_SLEEP_DURATION")
        return CommandType::SetSleepDuration;
    if (text == "SET_THRESHOLD")
        return CommandType::SetThreshold;
    if (text == "START_OTA")
        return CommandType::StartOta;
    if (text == "SLEEP_NOW")
        return CommandType::SleepNow;
    if (text == "START_PUMP")
        return CommandType::StartPump;
    if (text == "RESEND_CHUNK")
        return CommandType::ResendChunk;
    if (text == "SET_FILTER_MODE")
        return CommandType::SetFilterMode;
    if (text == "SET_PUMP_TIME")
        return CommandType::SetPumpTime;
    if (text == "SET_CONTROL_MODE")
        return CommandType::SetControlMode;
    if (text == "SET_DUTY_CYCLE")
        return CommandType::SetDutyCycle;
    return CommandType::None;
}

String encodeTelemetry(const TelemetryPacket &packet)
{
    String base = "TYPE=DATA";
    base += ",NODE=" + String(packet.nodeId);
    base += ",PID=" + String(packet.packetId);
    base += ",T=" + String(packet.temperatureC, 1);
    base += ",HA=" + String(packet.humidityRh, 1);
    base += ",HS=" + String(packet.soilMoistureVol, 0);
    base += ",VB=" + String(packet.batteryV, 2);
    base += ",ADC=" + String(packet.adcFiltered);
    base += ",SOIL=" + packet.soilStatus;
    base += ",ERR=" + String(packet.errorFlag);
    base.toUpperCase();
    base += ",CRC=" + String(crc16Ccitt(base), HEX);
    base.toUpperCase();
    return base;
}

String encodeAck(const AckPacket &packet)
{
    String base = "TYPE=ACK";
    base += ",NODE=" + String(packet.nodeId);
    base += ",PID=" + String(packet.packetId);
    base += ",OK=" + String(packet.ok ? 1 : 0);
    base += ",CMD=" + commandToText(packet.command);
    base += ",PARAM=" + String(packet.parameter);
    base += ",STATUS=" + packet.status;
    base.toUpperCase();
    base += ",CRC=" + String(crc16Ccitt(base), HEX);
    base.toUpperCase();
    return base;
}

String encodeOtaChunk(const OtaChunkPacket &packet)
{
    String base = "TYPE=OTA_CHUNK";
    base += ",NODE=" + String(packet.nodeId);
    base += ",OTAID=" + String(packet.otaId);
    base += ",IDX=" + String(packet.chunkIndex);
    base += ",TOTAL=" + String(packet.totalChunks);
    base += ",DATA=" + packet.payloadData;
    base += ",DCRC=" + String(packet.dataCrc, HEX);
    base.toUpperCase();
    base += ",CRC=" + String(crc16Ccitt(base), HEX);
    base.toUpperCase();
    return base;
}

String encodeOtaStatus(const OtaStatusPacket &packet)
{
    String base = "TYPE=OTA_STATUS";
    base += ",NODE=" + String(packet.nodeId);
    base += ",OTAID=" + String(packet.otaId);
    base += ",IDX=" + String(packet.chunkIndex);
    base += ",OK=" + String(packet.ok ? 1 : 0);
    base += ",STATUS=" + packet.status;
    base.toUpperCase();
    base += ",CRC=" + String(crc16Ccitt(base), HEX);
    base.toUpperCase();
    return base;
}

bool decodeTelemetry(const String &line, TelemetryPacket &packet)
{
    if (getField(line, "TYPE") != "DATA" || !hasValidCrc(line))
    {
        return false;
    }
    packet.nodeId = uint8_t(getField(line, "NODE").toInt());
    packet.packetId = uint32_t(getField(line, "PID").toInt());
    packet.temperatureC = getField(line, "T").toFloat();
    packet.humidityRh = getField(line, "HA").toFloat();
    packet.soilMoistureVol = getField(line, "HS").toFloat();
    packet.batteryV = getField(line, "VB").toFloat();
    packet.adcFiltered = getField(line, "ADC").toInt();
    packet.soilStatus = getField(line, "SOIL");
    packet.errorFlag = uint8_t(getField(line, "ERR").toInt());
    return packet.nodeId > 0 && packet.packetId > 0;
}

bool decodeAck(const String &line, AckPacket &packet)
{
    if (getField(line, "TYPE") != "ACK" || !hasValidCrc(line))
    {
        return false;
    }
    packet.nodeId = uint8_t(getField(line, "NODE").toInt());
    packet.packetId = uint32_t(getField(line, "PID").toInt());
    packet.ok = getField(line, "OK").toInt() == 1;
    packet.command = commandFromText(getField(line, "CMD"));
    packet.parameter = getField(line, "PARAM").toInt();
    packet.status = getField(line, "STATUS");
    return packet.nodeId > 0 && packet.packetId > 0;
}

bool decodeOtaChunk(const String &line, OtaChunkPacket &packet)
{
    if (getField(line, "TYPE") != "OTA_CHUNK" || !hasValidCrc(line))
    {
        return false;
    }
    packet.nodeId = uint8_t(getField(line, "NODE").toInt());
    packet.otaId = uint32_t(getField(line, "OTAID").toInt());
    packet.chunkIndex = uint16_t(getField(line, "IDX").toInt());
    packet.totalChunks = uint16_t(getField(line, "TOTAL").toInt());
    packet.payloadData = getField(line, "DATA");
    packet.dataCrc = uint16_t(strtoul(getField(line, "DCRC").c_str(), nullptr, 16));
    return packet.nodeId > 0 && packet.otaId > 0 && packet.totalChunks > 0;
}

bool decodeOtaStatus(const String &line, OtaStatusPacket &packet)
{
    if (getField(line, "TYPE") != "OTA_STATUS" || !hasValidCrc(line))
    {
        return false;
    }
    packet.nodeId = uint8_t(getField(line, "NODE").toInt());
    packet.otaId = uint32_t(getField(line, "OTAID").toInt());
    packet.chunkIndex = uint16_t(getField(line, "IDX").toInt());
    packet.ok = getField(line, "OK").toInt() == 1;
    packet.status = getField(line, "STATUS");
    return packet.nodeId > 0 && packet.otaId > 0;
}

bool hasValidCrc(const String &line)
{
    const String expected = getField(line, "CRC");
    if (expected.length() == 0)
    {
        return false;
    }
    const uint16_t actual = crc16Ccitt(withoutCrc(line));
    return uint16_t(strtoul(expected.c_str(), nullptr, 16)) == actual;
}

String getField(const String &line, const String &key)
{
    const String token = key + "=";
    int start = line.indexOf(token);
    if (start < 0)
    {
        return "";
    }
    start += token.length();
    int end = line.indexOf(',', start);
    if (end < 0)
    {
        end = line.length();
    }
    return line.substring(start, end);
}
