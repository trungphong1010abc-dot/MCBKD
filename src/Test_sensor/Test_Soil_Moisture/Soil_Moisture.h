#ifndef SOIL_MOISTURE_H 
#define SOIL_MOISTURE_H

#include <Arduino.h>

// Chân cảm biến
#define SOIL_PIN 34

// Cấu hình đọc mẫu
#define SAMPLE_COUNT 11 // Số mẫu để lọc median (nên là số lẻ)
#define DISCARD_COUNT 3 // Số mẫu đầu tiên bỏ qua để ổn định cảm biến sau khi bật nguồn
#define READ_DELAY_MS 10    // Thời gian delay giữa các lần đọc mẫu (ms)
#define LOOP_DELAY_MS 2000  // Thời gian delay giữa các lần đọc trong loop chính (ms)
#define SHORT_DELAY_MS 500 // Thời gian delay ngắn khi gặp lỗi ADC (ms)

// Giá trị hiệu chuẩn
extern int ADC_DRY;
extern int ADC_WET;

// Hàm dùng chung
void soilInit(); // Khởi tạo cảm biến
int readADCOnce(); // Đọc ADC một lần
bool isADCValid(int adc); // Kiểm tra giá trị ADC có hợp lệ không
int medianFilter(int arr[], int size); // Hàm lọc median
int readSoilMedian(); // Đọc nhiều mẫu và trả về giá trị đã lọc median
float adcToMoisturePercent(int adcFiltered); // Chuyển giá trị ADC đã lọc sang phần trăm độ ẩm

// Chế độ test
void soilTestSetup();   // Thiết lập cho chế độ test
void soilTestLoop();  // Vòng lặp chính cho chế độ test 

// Chế độ calib
void soilCalibSetup(); // Thiết lập cho chế độ calib    
void soilCalibLoop();   // Vòng lặp chính cho chế độ calib

#endif 