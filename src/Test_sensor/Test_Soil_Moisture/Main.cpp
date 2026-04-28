#include <Arduino.h>
#include "Soil_Moisture.h"

// Chọn 1 trong 2 chế độ dưới đây

//#define RUN_CALIB
#define RUN_TEST

void setup() {
#ifdef RUN_CALIB
  soilCalibSetup();
#endif

#ifdef RUN_TEST
  soilTestSetup();
#endif
}

void loop() {
#ifdef RUN_CALIB
  soilCalibLoop();
#endif

#ifdef RUN_TEST
  soilTestLoop();
#endif
}

/*
CÁCH DÙNG:

1. Chạy hiệu chuẩn khô:
   - Bật RUN_CALIB
   - Để cảm biến ngoài không khí hoặc đất khô
   - Mở Serial Monitor
   - Copy giá trị ADC_CALIB vào ADC_DRY trong Test.cpp

2. Chạy hiệu chuẩn ướt:
   - Vẫn bật RUN_CALIB
   - Cắm cảm biến vào đất ướt hoặc nước
   - Không ngâm quá vạch đỏ
   - Copy giá trị ADC_CALIB vào ADC_WET trong Test.cpp

(Lưu ý: ADC_DRY phải lớn hơn ADC_WET, chạy hiệu chuẩn khô trước để đảm bảo điều này)

3. Chạy test:
   - Comment RUN_CALIB
   - Bỏ comment RUN_TEST
   - Upload lại code

GIẢI THÍCH GIÁ TRỊ:

ADC check:
- Giá trị ADC thô đọc 1 lần
- Dùng để kiểm tra cảm biến có lỗi phần cứng không

ADC_filtered:
- Giá trị ADC sau lọc median
- Ổn định hơn ADC check
- Dùng để tính độ ẩm

Soil moisture H:
- Độ ẩm đất tính theo phần trăm

Buffer H:
- Giá trị độ ẩm lưu lại để sau này dùng cho IoT, Blynk, LoRa, vẽ graph

LƯU Ý:
- ADC_DRY phải lớn hơn ADC_WET
- Nếu ADC_filtered > ADC_DRY thì H bị ép về 0%
- Nếu ADC_filtered < ADC_WET thì H bị ép về 100%
*/