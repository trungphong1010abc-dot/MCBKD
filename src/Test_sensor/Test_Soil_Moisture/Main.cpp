#include <Arduino.h>
#include "soil_moisture.h"

// thay đổi giữa chế độ calib và test bằng cách comment/uncomment 2 dòng define dưới đây
//#define RUN_CALIB
#define RUN_TEST 

// ifdef và endif dùng để chọn code nào sẽ được biên dịch dựa trên việc RUN_CALIB hay RUN_TEST được định nghĩa
void setup() {
#ifdef RUN_CALIB 
  soilCalibSetup();
#else
  soilTestSetup();
#endif
}

void loop() {
#ifdef RUN_CALIB
  soilCalibLoop();
#else
  soilTestLoop();
#endif
}

/*
ADC check:
- Giá trị đọc thô 1 lần
- Dùng để kiểm tra cảm biến có lỗi không

ADC_filtered:
- Giá trị sau lọc median (ổn định hơn)
- Dùng để tính toán

Soil moisture H:
- Độ ẩm đất (%)
- Tính từ ADC_filtered

Buffer H:
- Lưu lại giá trị H để dùng sau (IoT, vẽ graph...)

LỖI HAY GẶP:
- ADC_filtered > ADC_DRY → H bị âm → ép về 0%
- => ADC_DRY đang set sai → cần calib lại
*/
