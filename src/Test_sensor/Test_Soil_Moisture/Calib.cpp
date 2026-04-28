#include "soil_moisture.h"

// ================== CONFIG ==================
#define CALIB_SAMPLE_COUNT 50
#define CALIB_READ_DELAY_MS 20
#define CALIB_LOOP_DELAY_MS 1000

// hàm đọc nhiều mẫu và trả về giá trị trung bình (dùng cho calib)
int readAverageADC() {
  long sum = 0;

  for (int i = 0; i < CALIB_SAMPLE_COUNT; i++) {
    sum += analogRead(SOIL_PIN);
    delay(CALIB_READ_DELAY_MS);
  }

  return sum / CALIB_SAMPLE_COUNT;
}

// hàm chuyển giá trị ADC đã lọc sang phần trăm độ ẩm
void soilCalibSetup() {
  Serial.begin(115200);
  delay(1000);

  soilInit();    // Khởi tạo cảm biến
 
  // Hướng dẫn calib
  Serial.println("=================================");
  Serial.println("CALIB SOIL MOISTURE SENSOR");
  Serial.println("=================================");
  Serial.println("1. De cam bien ngoai khong khi/dat kho -> lay ADC_DRY");
  Serial.println("2. Cam cam bien vao nuoc/dat rat uot -> lay ADC_WET");
  Serial.println("3. Khong ngam qua vach do tren cam bien.");
}

// hàm chuyển giá trị ADC đã lọc sang phần trăm độ ẩm
void soilCalibLoop() {
  int adcAvg = readAverageADC();

  Serial.print("ADC average = ");
  Serial.println(adcAvg);

  delay(CALIB_LOOP_DELAY_MS);
}

/*
Giá trị calib thu được:
- ADC_DRY: 3400 (giá trị ADC cao nhất đo được khi cảm biến ở ngoài không khí hoặc đất khô)
- ADC_WET: 1000 (giá trị ADC thấp nhất đo được khi cảm biến ở trong nước hoặc đất rất ẩm)
*/