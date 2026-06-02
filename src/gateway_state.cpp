#include "gateway_state.h"

StoredTelemetry history[kGatewayHistorySize];
size_t historyCount = 0;
size_t historyWriteIndex = 0;
uint32_t lastPacketIdByNode[256] = {};
AckPacket pendingCommands[256];
uint8_t pendingCommandRepeats[256] = {};
portMUX_TYPE pendingCommandMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t lastHeartbeatMs = 0;
uint32_t lastLoraRetryMs = 0;
bool loraReady = false;
bool otaSessionActive = false;
bool otaReadyWaitActive = false;
uint8_t otaReadyWaitNodeId = 0;
uint32_t otaReadyWaitId = 0;
uint32_t otaReadyWaitDeadlineMs = 0;
