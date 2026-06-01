#include "soil_moisture.h"

#include <math.h>

void SoilMoistureSensor::begin(uint8_t adcPin, int adcDry, int adcWet, int adcMin, int adcMax)
{
    _adcPin = adcPin;
    _adcDry = adcDry;
    _adcWet = adcWet;
    _adcMin = adcMin;
    _adcMax = adcMax;
    analogReadResolution(12);
    analogSetPinAttenuation(_adcPin, ADC_11db);
}

SoilReading SoilMoistureSensor::read()
{
    SoilReading reading;
    int samples[kSamples - kDiscardFirst] = {};

    for (size_t i = 0; i < kSamples; i++)
    {
        const int adc = analogRead(_adcPin);
        reading.adcRaw = adc;

        if (adc <= _adcMin || adc >= _adcMax)
        {
            _adcErrorCount++;
            if (_adcErrorCount >= kMaxAdcErrors)
            {
                reading.soilStatus = SoilStatus::Error;
                reading.sensorStatus = SensorStatus::Error;
                reading.errorFlag = 1;
                return reading;
            }
            delay(350);
            i--;
            continue;
        }

        if (i >= kDiscardFirst)
        {
            samples[i - kDiscardFirst] = adc;
        }
        delay(20);
    }

    _adcErrorCount = 0;
    reading.adcFiltered = median(samples, kSamples - kDiscardFirst);
    reading.moistureVol = clampMoisture(
        (_adcDry - reading.adcFiltered) * 60.0f / float(_adcDry - _adcWet));
    reading.soilStatus = classify(reading.moistureVol);
    reading.sensorStatus = SensorStatus::Ok;
    reading.errorFlag = 0;
    return reading;
}

const char *SoilMoistureSensor::statusText(SoilStatus status)
{
    switch (status)
    {
    case SoilStatus::UrgentWatering:
        return "URGENT_WATERING";
    case SoilStatus::NeedWatering:
        return "NEED_WATERING";
    case SoilStatus::LightDry:
        return "LIGHT_DRY";
    case SoilStatus::Normal:
        return "NORMAL";
    case SoilStatus::OverMoisture:
        return "OVER_MOISTURE";
    default:
        return "ERROR";
    }
}

SoilStatus SoilMoistureSensor::classify(float moistureVol)
{
    if (isnan(moistureVol))
    {
        return SoilStatus::Error;
    }
    if (moistureVol > 42.0f)
    {
        return SoilStatus::OverMoisture;
    }
    if (moistureVol > 33.0f)
    {
        return SoilStatus::Normal;
    }
    if (moistureVol > 24.0f)
    {
        return SoilStatus::LightDry;
    }
    if (moistureVol > 15.0f)
    {
        return SoilStatus::NeedWatering;
    }
    return SoilStatus::UrgentWatering;
}

int SoilMoistureSensor::median(int *values, size_t length)
{
    for (size_t i = 1; i < length; i++)
    {
        const int key = values[i];
        int j = int(i) - 1;
        while (j >= 0 && values[j] > key)
        {
            values[j + 1] = values[j];
            j--;
        }
        values[j + 1] = key;
    }
    return values[length / 2];
}

float SoilMoistureSensor::clampMoisture(float moistureVol)
{
    if (moistureVol < 0.0f)
    {
        return 0.0f;
    }
    if (moistureVol > 60.0f)
    {
        return 60.0f;
    }
    return roundf(moistureVol * 10.0f) / 10.0f;
}
