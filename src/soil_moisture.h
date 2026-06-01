#pragma once

#include <Arduino.h>

#include "dht22_sensor.h"

enum class SoilStatus : uint8_t
{
    UrgentWatering = 0,
    NeedWatering = 1,
    LightDry = 2,
    Normal = 3,
    OverMoisture = 4,
    Error = 5
};

struct SoilReading
{
    int adcRaw = 0;
    int adcFiltered = 0;
    float moistureVol = NAN;
    SoilStatus soilStatus = SoilStatus::Error;
    SensorStatus sensorStatus = SensorStatus::Error;
    uint8_t errorFlag = 1;
};

class SoilMoistureSensor
{
public:
    void begin(uint8_t adcPin, int adcDry, int adcWet, int adcMin, int adcMax);
    SoilReading read();

    static const char *statusText(SoilStatus status);
    static SoilStatus classify(float moistureVol);

private:
    static constexpr size_t kSamples = 11;
    static constexpr size_t kDiscardFirst = 3;
    static constexpr uint8_t kMaxAdcErrors = 3;

    uint8_t _adcPin = 0;
    int _adcDry = 3400;
    int _adcWet = 1200;
    int _adcMin = 100;
    int _adcMax = 4090;
    uint8_t _adcErrorCount = 0;

    static int median(int *values, size_t length);
    static float clampMoisture(float moistureVol);
};
