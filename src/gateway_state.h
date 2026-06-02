#pragma once

#include <Arduino.h>

#include "packet_protocol.h"

struct StoredTelemetry
{
    TelemetryPacket packet;
    int rssi = 0;
    float snr = 0.0f;
    uint32_t receivedMs = 0;
};

constexpr size_t kGatewayHistorySize = 64;

extern StoredTelemetry history[kGatewayHistorySize];
extern size_t historyCount;
extern size_t historyWriteIndex;
extern uint32_t lastPacketIdByNode[256];
extern AckPacket pendingCommands[256];
extern uint8_t pendingCommandRepeats[256];
extern portMUX_TYPE pendingCommandMux;
extern uint32_t lastHeartbeatMs;
extern uint32_t lastLoraRetryMs;
extern bool loraReady;
extern bool otaSessionActive;
extern bool otaReadyWaitActive;
extern uint8_t otaReadyWaitNodeId;
extern uint32_t otaReadyWaitId;
extern uint32_t otaReadyWaitDeadlineMs;
