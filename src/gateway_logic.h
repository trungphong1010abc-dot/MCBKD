#pragma once

#include <Arduino.h>

#include "packet_protocol.h"

int parseCommandParameter(CommandType command, String value);
int computePumpTime(const TelemetryPacket &packet);
String alertLevel(const TelemetryPacket &packet);
void storeTelemetry(const TelemetryPacket &packet, int rssi, float snr);
void queueGatewayCommand(uint8_t nodeId, CommandType command, int parameter);
