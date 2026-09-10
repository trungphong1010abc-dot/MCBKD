#pragma once

#include <Arduino.h>

namespace Config
{
constexpr uint32_t SerialBaud = 115200;
constexpr char GatewayFirmwareVersion[] = "GW_1";

constexpr uint8_t NodeId = 1;
constexpr uint8_t GatewayId = 1;

constexpr uint8_t DhtPin = 27;
constexpr uint8_t SoilAdcPin = 34;
constexpr uint8_t SensorPowerPin = 32;
constexpr uint8_t PumpPin = 25;
constexpr bool PumpActiveHigh = true;
constexpr uint8_t MaxPumpSeconds = 15;
constexpr bool EnablePumpHardware = false;

constexpr float TempOffsetC = 0.0f;
constexpr float HumidityOffsetRh = 0.0f;
constexpr int SoilAdcDry = 3400;
constexpr int SoilAdcWet = 1200;
constexpr int SoilAdcMin = 100;
constexpr int SoilAdcMax = 4090;

constexpr uint8_t LoraSs = 5;
constexpr uint8_t LoraRst = 14;
constexpr uint8_t LoraDio0 = 26;
constexpr uint8_t LoraSck = 18;
constexpr uint8_t LoraMiso = 19;
constexpr uint8_t LoraMosi = 23;
constexpr long LoraFrequency = 433E6;
constexpr uint8_t LoraSpreadingFactor = 7;
constexpr long LoraSignalBandwidth = 125E3;
constexpr uint8_t LoraCodingRate = 5;
constexpr uint8_t LoraTxPowerDbm = 17;

constexpr uint16_t AckTimeoutMs = 2500;
constexpr uint16_t CommandDedupWindow = 32;

constexpr bool EnableDeepSleep = true;
constexpr uint32_t DefaultSleepMinutes = 30;

constexpr char WifiSsid[] = "Khoa";
constexpr char WifiPassword[] = "12112004";
constexpr char ThingsBoardHost[] = "eu.thingsboard.cloud";
constexpr char ThingsBoardToken[] = "QYtNEEPckyFjPteWRQ9o";
constexpr uint16_t ThingsBoardHttpPort = 80;
constexpr bool EnableCloudUpload = true;

constexpr bool EnableGatewayHttpDebug = false;
constexpr bool EnableGatewayHeartbeatLog = false;
}
