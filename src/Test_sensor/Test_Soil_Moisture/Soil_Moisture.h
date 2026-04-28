#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include <Arduino.h>

// ================== CHÂN CẢM BIẾN ==================
#define SOIL_PIN 34

// ================== CẤU HÌNH ĐỌC MẪU ==================
#define SAMPLE_COUNT 11
#define DISCARD_COUNT 3
#define READ_DELAY_MS 10

#define LOOP_DELAY_MS 2000
#define SHORT_DELAY_MS 500

// ================== GIÁ TRỊ HIỆU CHUẨN ==================
extern int ADC_DRY;
extern int ADC_WET;

// ================== HÀM DÙNG CHUNG ==================
void soilInit();

int readADCOnce();
bool isADCValid(int adc);

int medianFilter(int arr[], int size);
int readSoilMedian();

float adcToMoisturePercent(int adcFiltered);

// ================== TEST MODE ==================
void soilTestSetup();
void soilTestLoop();

// ================== CALIB MODE ==================
void soilCalibSetup();
void soilCalibLoop();

#endif