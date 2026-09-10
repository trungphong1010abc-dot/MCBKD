#include "gateway_logic.h"

#include "gateway_state.h"

int parseCommandParameter(CommandType command, String value)
{
    value.trim();
    value.toUpperCase();
    if (command == CommandType::SetFilterMode)
    {
        return value == "MEDIAN" ? 1 : 0;
    }
    if (command == CommandType::SetControlMode)
    {
        return value == "AUTO" ? 1 : 0;
    }
    if (command == CommandType::SetDutyCycle)
    {
        return value == "ADAPTIVE" ? 1 : 0;
    }
    return value.toInt();
}

int computePumpTime(const TelemetryPacket &packet)
{
    if (packet.errorFlag != 0)
    {
        return 0;
    }

    int pumpTime = 0;
    if (packet.soilMoistureVol > 42.0f)
    {
        pumpTime = 0;
    }
    else if (packet.soilMoistureVol > 33.0f)
    {
        pumpTime = 0;
    }
    else if (packet.soilMoistureVol > 24.0f)
    {
        pumpTime = 3;
    }
    else if (packet.soilMoistureVol > 15.0f)
    {
        pumpTime = 6;
    }
    else
    {
        pumpTime = 10;
    }

    if (packet.temperatureC > 32.0f)
    {
        pumpTime += 2;
    }
    if (packet.humidityRh < 50.0f)
    {
        pumpTime += 3;
    }
    if (packet.humidityRh > 80.0f)
    {
        pumpTime -= 2;
    }
    return constrain(pumpTime, 0, 15);
}

String alertLevel(const TelemetryPacket &packet)
{
    if (packet.errorFlag != 0)
    {
        return "SENSOR_ERROR";
    }
    if (packet.soilMoistureVol > 42.0f)
    {
        return "OVER_MOISTURE";
    }
    if (packet.soilMoistureVol > 33.0f)
    {
        return "NORMAL";
    }
    if (packet.soilMoistureVol > 24.0f)
    {
        return "LIGHT_DRY";
    }
    if (packet.soilMoistureVol > 15.0f)
    {
        return "NEED_WATERING";
    }
    return "URGENT_WATERING";
}

void storeTelemetry(const TelemetryPacket &packet, int rssi, float snr)
{
    history[historyWriteIndex].packet = packet;
    history[historyWriteIndex].rssi = rssi;
    history[historyWriteIndex].snr = snr;
    history[historyWriteIndex].receivedMs = millis();
    historyWriteIndex = (historyWriteIndex + 1) % kGatewayHistorySize;
    if (historyCount < kGatewayHistorySize)
    {
        historyCount++;
    }
}

void queueGatewayCommand(uint8_t nodeId, CommandType command, int parameter)
{
    portENTER_CRITICAL(&pendingCommandMux);
    pendingCommands[nodeId].nodeId = nodeId;
    pendingCommands[nodeId].command = command;
    pendingCommands[nodeId].parameter = parameter;
    pendingCommands[nodeId].ok = true;
    pendingCommands[nodeId].status = "QUEUED";
    pendingCommandRepeats[nodeId] = 3;
    portEXIT_CRITICAL(&pendingCommandMux);
}
