#include "Soil_Moisture.h"

// ================== CONFIG CALIB ==================
#define CALIB_SAMPLE_COUNT 120
#define CALIB_READ_DELAY_MS 20
#define CALIB_LOOP_DELAY_MS 2000
#define CALIB_STABILIZE_DELAY_MS 5000

// Hàm đọc ADC để hiệu chuẩn cực ổn định
// Cách làm:
// 1. Chờ cảm biến ổn định 5s
// 2. Đọc 120 mẫu
// 3. Bỏ mẫu lỗi ADC
// 4. Sort tăng dần
// 5. Bỏ 10% thấp nhất và 10% cao nhất
// 6. Lấy trung bình phần còn lại
int readCalibrationADC() {
  const int N = CALIB_SAMPLE_COUNT;
  int samples[N];
  int count = 0;

  Serial.println();
  Serial.println("Dang cho cam bien on dinh...");
  delay(CALIB_STABILIZE_DELAY_MS);

  while (count < N) {
    int adc = analogRead(SOIL_PIN);

    if (isADCValid(adc)) {
      samples[count] = adc;
      count++;
    }

    delay(CALIB_READ_DELAY_MS);
  }

  // Sort tang dan
  for (int i = 0; i < N - 1; i++) {
    for (int j = i + 1; j < N; j++) {
      if (samples[j] < samples[i]) {
        int temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }

  // Bo 10% dau va 10% cuoi de loai nhieu/outlier
  int trim = N / 10;
  long sum = 0;
  int validCount = 0;

  for (int i = trim; i < N - trim; i++) {
    sum += samples[i];
    validCount++;
  }

  int adcCalib = sum / validCount;

  Serial.println("----- CALIB DETAIL -----");
  Serial.print("Min ADC: ");
  Serial.println(samples[0]);

  Serial.print("Max ADC: ");
  Serial.println(samples[N - 1]);

  Serial.print("ADC calib result: ");
  Serial.println(adcCalib);
  Serial.println("------------------------");

  return adcCalib;
}

void soilCalibSetup() {
  Serial.begin(115200);
  delay(1000);

  soilInit();

  Serial.println("=================================");
  Serial.println("CALIB SOIL MOISTURE SENSOR");
  Serial.println("=================================");
  Serial.println("Huong dan:");
  Serial.println("1. De cam bien trong DAT KHO / ngoai khong khi de lay ADC_DRY.");
  Serial.println("2. Cam cam bien vao DAT UOT / nuoc de lay ADC_WET.");
  Serial.println("3. Khong ngam qua vach do tren cam bien.");
  Serial.println("4. Gia tri in ra la gia tri nen copy vao ADC_DRY hoac ADC_WET.");
  Serial.println("=================================");
}

void soilCalibLoop() {
  int adcCalib = readCalibrationADC();

  Serial.print("ADC_CALIB = ");
  Serial.println(adcCalib);

  Serial.println();
  Serial.println("Neu dang de cam bien kho:");
  Serial.print("int ADC_DRY = ");
  Serial.print(adcCalib);
  Serial.println(";");

  Serial.println("Neu dang de cam bien uot:");
  Serial.print("int ADC_WET = ");
  Serial.print(adcCalib);
  Serial.println(";");

  Serial.println();

  delay(CALIB_LOOP_DELAY_MS);
}