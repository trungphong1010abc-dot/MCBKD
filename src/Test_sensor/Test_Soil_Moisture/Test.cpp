#include "soil_moisture.h"

// ================== CONFIG ==================
int ADC_DRY = 3400; // Cần hiệu chỉnh lại giá trị này dựa trên thực tế của cảm biến và môi trường sử dụng
int ADC_WET = 1000;

// giá trị ADC hợp lệ (dựa trên thực tế đo được, có thể cần điều chỉnh)
const int ADC_MIN = 100;
const int ADC_MAX = 4000;
const int MAX_ADC_ERROR = 3; // Số lần đọc ADC liên tiếp không hợp lệ trước khi báo lỗi phần cứng

// Biến toàn cục
int adcErrorCount = 0; // Đếm số lần đọc ADC không hợp lệ liên tiếp
float soilBuffer = 0.0; // Buffer lưu giá trị độ ẩm đất (có thể dùng để IoT, vẽ graph...)

// ================== HÀM DÙNG CHUNG ==================

// Hàm khởi tạo cảm biến
void soilInit() {
  pinMode(SOIL_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
}

// Hàm đọc ADC một lần
int readADCOnce() {
  return analogRead(SOIL_PIN);
}

// Hàm kiểm tra giá trị ADC có hợp lệ không
bool isADCValid(int adc) { 
  return adc > ADC_MIN && adc < ADC_MAX;
}

// Hàm lọc median
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

// Hàm đọc nhiều mẫu và trả về giá trị đã lọc median
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

// Hàm chuyển giá trị ADC đã lọc sang phần trăm độ ẩm
float adcToMoisturePercent(int adcFiltered) {
  float H = (float)(ADC_DRY - adcFiltered) * 100.0 / (ADC_DRY - ADC_WET);

  if (H < 0) H = 0;
  if (H > 100) H = 100;

  return H;
}

// ================== TEST MODE ==================

// Hàm thiết lập cho chế độ test
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

// Vòng lặp chính cho chế độ test
void soilTestLoop() {
  Serial.println("-----------------------------");

  int adcCheck = readADCOnce();

  Serial.print("ADC check: "); // Giá trị ADC đọc thô 1 lần để kiểm tra cảm biến có lỗi không
  Serial.println(adcCheck);

  if (!isADCValid(adcCheck)) {
    adcErrorCount++;

    Serial.print("ADC invalid. Error count = ");
    Serial.println(adcErrorCount);

    if (adcErrorCount > MAX_ADC_ERROR) {
      Serial.println("HARDWARE ERROR!");
      Serial.println("SYSTEM STOPPED.");

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