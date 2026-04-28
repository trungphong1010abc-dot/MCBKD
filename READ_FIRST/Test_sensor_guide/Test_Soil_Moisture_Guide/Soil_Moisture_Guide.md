# Giải thích flowchart soil_moisture

## Wiring

VCC  -> 3.3V ESP32
GND  -> GND ESP32
AOUT -> GPIO34

![alt text](<Soil_Moisture.jpg>)

## Quy trình thuật toán đo độ ẩm đất bằng cảm biến điện dung

Chương trình được tách thành 4 file để dễ quản lý. File `soil_moisture.h` khai báo các chân, thông số lấy mẫu và nguyên mẫu hàm. File `test.cpp` chứa thuật toán đo độ ẩm chính. File `calib.cpp` dùng để hiệu chuẩn, lấy hai giá trị `ADC_DRY` và `ADC_WET`. File `main.cpp` là file điều khiển, chọn chạy chế độ test hoặc calib bằng cách bật/tắt `RUN_CALIB` và `RUN_TEST`.

Khi chạy chương trình, ESP32 bắt đầu từ `main.cpp`. Nếu chọn chế độ calib, hệ thống gọi `soilCalibSetup()` và `soilCalibLoop()`. Nếu chọn chế độ test, hệ thống gọi `soilTestSetup()` và `soilTestLoop()`.

Ở chế độ test, hệ thống khởi tạo Serial, cấu hình chân đọc cảm biến `GPIO34`, cấu hình ADC độ phân giải 12-bit và mức suy hao `ADC_11db`. Sau đó chương trình kiểm tra điều kiện hiệu chuẩn, đảm bảo `ADC_DRY > ADC_WET`, vì cảm biến độ ẩm đất điện dung thường cho giá trị ADC cao khi khô và thấp khi ướt.

Trong mỗi chu kỳ đo, ESP32 đọc nhanh một mẫu ADC thô bằng `readADCOnce()`. Giá trị này được dùng để kiểm tra cảm biến có hoạt động bình thường không. Nếu ADC không nằm trong khoảng hợp lệ `ADC_MIN < ADC < ADC_MAX`, bộ đếm lỗi `adcErrorCount` tăng lên. Nếu lỗi liên tiếp vượt quá 3 lần, hệ thống báo lỗi phần cứng và dừng. Nếu lỗi chưa vượt ngưỡng, chương trình delay ngắn 300–500 ms rồi quay lại đọc ADC.

Nếu ADC hợp lệ, bộ đếm lỗi được reset về 0. Sau đó chương trình bỏ qua một số mẫu đầu để ổn định tín hiệu, rồi đọc 11 mẫu ADC. Các mẫu này được đưa qua bộ lọc median: sắp xếp dãy mẫu và lấy giá trị ở giữa. Kết quả thu được là `ADC_filtered`, giúp giảm nhiễu đột biến và ổn định dữ liệu đo.

Tiếp theo, chương trình chuyển `ADC_filtered` sang độ ẩm đất theo công thức:

```text
H = (ADC_DRY - ADC_filtered) × 100 / (ADC_DRY - ADC_WET)
```

Trong đó, `ADC_DRY` là giá trị ADC khi cảm biến ở trạng thái khô, còn `ADC_WET` là giá trị ADC khi cảm biến ở trạng thái ướt. Sau khi tính, giá trị H được giới hạn trong khoảng 0–100% để tránh sai số ngoài phạm vi vật lý.

Cuối cùng, giá trị độ ẩm `H` được lưu vào biến `soilBuffer`, sau đó xuất ra Serial gồm `ADC_filtered`, `Soil moisture H (%)` và `Buffer H`. Hệ thống delay 2 giây rồi quay lại chu kỳ đo tiếp theo.

Ở chế độ calib, chương trình không tính độ ẩm mà chỉ đọc trung bình 50 mẫu ADC và in ra Serial. Người dùng đặt cảm biến ở ngoài không khí hoặc đất khô để lấy `ADC_DRY`, sau đó đặt cảm biến vào nước hoặc đất rất ẩm để lấy `ADC_WET`. Hai giá trị này được nhập lại vào file test để chương trình đo độ ẩm chính xác hơn.

Tóm lại, thuật toán gồm các bước chính: **khởi tạo cảm biến → kiểm tra ADC → xử lý lỗi nếu có → đọc nhiều mẫu → lọc median → tính độ ẩm → giới hạn kết quả → lưu buffer → xuất dữ liệu → lặp lại chu kỳ đo**.
