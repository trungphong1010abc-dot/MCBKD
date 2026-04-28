#include "Soil_Moisture.h"

// ================== GIÁ TRỊ HIỆU CHUẨN ==================
// Sau khi chạy RUN_CALIB, copy giá trị ADC_DRY và ADC_WET mới vào đây.
int ADC_DRY = 3080;
int ADC_WET = 1065;

// ================== NGƯỠNG ADC HỢP LỆ ==================
const int ADC_MIN = 100;
const int ADC_MAX = 4080; // không chọn 4095 vì có thể có lỗi phần cứng làm ADC đọc max luôn

const int MAX_ADC_ERROR = 3;

// ================== BIẾN TOÀN CỤC ==================
int adcErrorCount = 0;
float soilBuffer = 0.0;

// ================== HÀM DÙNG CHUNG ==================

void soilInit() {
  pinMode(SOIL_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);

  delay(1000);
}

int readADCOnce() {
  return analogRead(SOIL_PIN);
}

bool isADCValid(int adc) {
  return adc > ADC_MIN && adc < ADC_MAX;
}

int medianFilter(int arr[], int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (arr[j] < arr[i]) {
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }

  return arr[size / 2];
}

int readSoilMedian() {
  for (int i = 0; i < DISCARD_COUNT; i++) {
    analogRead(SOIL_PIN);
    delay(READ_DELAY_MS);
  }

  int samples[SAMPLE_COUNT];

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    samples[i] = analogRead(SOIL_PIN);
    delay(READ_DELAY_MS);
  }

  return medianFilter(samples, SAMPLE_COUNT);
}

float adcToMoisturePercent(int adcFiltered) {
  float H = (float)(ADC_DRY - adcFiltered) * 100.0 /
            (float)(ADC_DRY - ADC_WET);

  if (H < 0) H = 0;
  if (H > 100) H = 100;

  return H;
}

// ================== TEST MODE ==================

void soilTestSetup() {
  Serial.begin(115200);
  delay(1000);

  soilInit();

  Serial.println("=================================");
  Serial.println("TEST CAM BIEN DO AM DAT DIEN DUNG");
  Serial.println("ESP32 + Capacitive Soil Moisture");
  Serial.println("=================================");

  if (ADC_DRY <= ADC_WET) {
    Serial.println("ERROR: ADC_DRY must be greater than ADC_WET.");
    while (true) delay(1000);
  }

  Serial.print("SOIL_PIN: GPIO");
  Serial.println(SOIL_PIN);

  Serial.print("ADC_DRY: ");
  Serial.println(ADC_DRY);

  Serial.print("ADC_WET: ");
  Serial.println(ADC_WET);

  Serial.println("System ready.");
}

void soilTestLoop() {
  Serial.println("-----------------------------");

  int adcCheck = readADCOnce();

  Serial.print("ADC check: ");
  Serial.println(adcCheck);

  if (!isADCValid(adcCheck)) {
    adcErrorCount++;

    Serial.print("ADC invalid. Error count = ");
    Serial.println(adcErrorCount);

    if (adcErrorCount > MAX_ADC_ERROR) {
      Serial.println("HARDWARE ERROR!");
      Serial.println("SYSTEM STOPPED.");
      Serial.println("Check VCC, GND, OUT, ADC pin.");

      while (true) delay(1000);
    }

    delay(SHORT_DELAY_MS);
    return;
  }

  adcErrorCount = 0;

  int adcFiltered = readSoilMedian();
  float H = adcToMoisturePercent(adcFiltered);

  soilBuffer = H;

  Serial.print("ADC_filtered: ");
  Serial.println(adcFiltered);

  Serial.print("Soil moisture H: ");
  Serial.print(H, 1);
  Serial.println(" %");

  Serial.print("Buffer H: ");
  Serial.print(soilBuffer, 1);
  Serial.println(" %");

  delay(LOOP_DELAY_MS);
}