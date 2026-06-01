#include "dht22_sensor.h"

#include <math.h>

void Dht22Sensor::begin(uint8_t pin, float tempOffsetC, float humidityOffsetRh)
{
    _pin = pin;
    _tempOffsetC = tempOffsetC;
    _humidityOffsetRh = humidityOffsetRh;
    pinMode(_pin, INPUT_PULLUP);
    _started = true;
}

DhtReading Dht22Sensor::read()
{
    if (!_started)
    {
        return fail();
    }

    uint8_t bytes[5] = {};
    if (!readRaw40Bits(bytes))
    {
        return fail();
    }

    const uint8_t checksum = (bytes[0] + bytes[1] + bytes[2] + bytes[3]) & 0xFF;
    if (checksum != bytes[4])
    {
        return fail();
    }

    const uint16_t rawHumidity = (uint16_t(bytes[0]) << 8) | bytes[1];
    uint16_t rawTemperature = (uint16_t(bytes[2]) << 8) | bytes[3];
    const bool negative = (rawTemperature & 0x8000) != 0;
    rawTemperature &= 0x7FFF;

    float humidityRh = rawHumidity / 10.0f + _humidityOffsetRh;
    float temperatureC = rawTemperature / 10.0f + _tempOffsetC;
    if (negative)
    {
        temperatureC = -temperatureC;
    }

    if (temperatureC < -10.0f || temperatureC > 50.0f || humidityRh < 0.0f || humidityRh > 100.0f)
    {
        return fail();
    }

    if (_hasPrevious &&
        (fabsf(temperatureC - _previousTemperatureC) > 2.0f ||
         fabsf(humidityRh - _previousHumidityRh) > 5.0f))
    {
        return fail();
    }

    pushSample(temperatureC, humidityRh);
    _previousTemperatureC = temperatureC;
    _previousHumidityRh = humidityRh;
    _hasPrevious = true;

    DhtReading reading;
    reading.temperatureC = roundf(average(_tempBuffer) * 10.0f) / 10.0f;
    reading.humidityRh = roundf(average(_humidityBuffer) * 10.0f) / 10.0f;
    reading.status = SensorStatus::Ok;
    reading.errorFlag = 0;
    return reading;
}

bool Dht22Sensor::readRaw40Bits(uint8_t bytes[5])
{
    pinMode(_pin, OUTPUT_OPEN_DRAIN);
    digitalWrite(_pin, LOW);
    delay(20);
    digitalWrite(_pin, HIGH);
    delayMicroseconds(40);
    pinMode(_pin, INPUT_PULLUP);

    uint32_t duration = 0;
    if (!expectPulse(LOW, 100, duration) || !expectPulse(HIGH, 100, duration))
    {
        return false;
    }

    for (uint8_t bitIndex = 0; bitIndex < 40; bitIndex++)
    {
        if (!expectPulse(LOW, 80, duration) || !expectPulse(HIGH, 100, duration))
        {
            return false;
        }

        if (duration > 50)
        {
            bytes[bitIndex / 8] |= (1 << (7 - (bitIndex % 8)));
        }
    }

    return true;
}

bool Dht22Sensor::expectPulse(uint8_t level, uint32_t timeoutUs, uint32_t &durationUs) const
{
    const uint32_t start = micros();
    while (digitalRead(_pin) != level)
    {
        if (micros() - start > timeoutUs)
        {
            return false;
        }
    }

    const uint32_t pulseStart = micros();
    while (digitalRead(_pin) == level)
    {
        if (micros() - pulseStart > timeoutUs)
        {
            return false;
        }
    }

    durationUs = micros() - pulseStart;
    return true;
}

DhtReading Dht22Sensor::fail() const
{
    DhtReading reading;
    reading.status = SensorStatus::Error;
    reading.errorFlag = 1;
    return reading;
}

void Dht22Sensor::pushSample(float temperatureC, float humidityRh)
{
    _tempBuffer[_bufferIndex] = temperatureC;
    _humidityBuffer[_bufferIndex] = humidityRh;
    _bufferIndex = (_bufferIndex + 1) % kAverageSize;
    if (_bufferCount < kAverageSize)
    {
        _bufferCount++;
    }
}

float Dht22Sensor::average(const float *values) const
{
    float sum = 0.0f;
    for (size_t i = 0; i < _bufferCount; i++)
    {
        sum += values[i];
    }
    return _bufferCount == 0 ? NAN : sum / _bufferCount;
}
