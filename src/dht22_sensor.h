#pragma once

#include <Arduino.h>

enum class SensorStatus : uint8_t
{
    Ok = 0,
    Error = 1
};

struct DhtReading
{
    float temperatureC = NAN;
    float humidityRh = NAN;
    SensorStatus status = SensorStatus::Error;
    uint8_t errorFlag = 1;
};

class Dht22Sensor
{
public:
    void begin(uint8_t pin, float tempOffsetC, float humidityOffsetRh);
    DhtReading read();

private:
    static constexpr size_t kAverageSize = 5;

    uint8_t _pin = 0;
    float _tempOffsetC = 0.0f;
    float _humidityOffsetRh = 0.0f;
    bool _started = false;
    bool _hasPrevious = false;
    float _previousTemperatureC = 0.0f;
    float _previousHumidityRh = 0.0f;
    float _tempBuffer[kAverageSize] = {};
    float _humidityBuffer[kAverageSize] = {};
    size_t _bufferCount = 0;
    size_t _bufferIndex = 0;

    bool readRaw40Bits(uint8_t bytes[5]);
    bool expectPulse(uint8_t level, uint32_t timeoutUs, uint32_t &durationUs) const;
    DhtReading fail() const;
    void pushSample(float temperatureC, float humidityRh);
    float average(const float *values) const;
};
