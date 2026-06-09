# B4. Thiết Kế Phần Mềm Cho Hệ Thống

Tài liệu này mô tả thiết kế phần mềm của hệ thống mạng cảm biến không dây EE4552. Nội dung được viết theo code hiện tại trong thư mục `src/`, dùng để đưa vào báo cáo đồ án.

Hệ thống gồm hai firmware chính:

- **Node cảm biến**: đọc dữ liệu môi trường, gửi telemetry qua LoRa, nhận ACK/command từ gateway và ngủ sâu để tiết kiệm năng lượng.
- **Gateway**: nhận dữ liệu LoRa từ node, kiểm tra gói tin, lưu lịch sử, hiển thị dashboard web, xuất CSV, gửi dữ liệu lên ThingsBoard, gửi command ngược lại cho node và hỗ trợ OTA cho gateway.

---

## B4.1. Tổng Quan Kiến Trúc Phần Mềm

Phần mềm được thiết kế theo kiến trúc phân tách module. Mỗi nhóm chức năng được đặt trong một file riêng để dễ kiểm thử, bảo trì và mở rộng.

```text
+----------------------+        LoRa         +--------------------------+
|      Sensor Node     | ------------------> |         Gateway          |
|----------------------|                     |--------------------------|
| DHT22 Sensor         |                     | LoRa Receiver            |
| Soil Moisture Sensor |                     | Packet Decode + CRC      |
| Battery Measurement  |                     | Telemetry History        |
| Packet Encoder       |                     | Web Dashboard            |
| ACK/Command Handler  | <------------------ | ACK/Command Sender       |
| Deep Sleep Manager   |        LoRa ACK      | ThingsBoard Upload       |
+----------------------+                     | Gateway Self-OTA         |
                                             +--------------------------+
```

Luồng dữ liệu chính của hệ thống:

1. Node thức dậy sau chu kỳ deep sleep.
2. Node cấp nguồn cho cảm biến.
3. Node đọc nhiệt độ, độ ẩm không khí, độ ẩm đất và điện áp pin.
4. Node đóng gói dữ liệu thành gói `DATA` có CRC.
5. Node gửi gói `DATA` qua LoRa cho gateway.
6. Gateway nhận gói, kiểm tra CRC và decode telemetry.
7. Gateway lưu dữ liệu vào history RAM, hiển thị lên dashboard và upload ThingsBoard nếu WiFi sẵn sàng.
8. Gateway gửi ACK về node, có thể kèm command cấu hình.
9. Node nhận ACK, thực thi command nếu có.
10. Node tính thời gian ngủ và vào deep sleep.

Thiết kế này giúp node tiêu thụ năng lượng thấp vì node chỉ hoạt động trong thời gian ngắn để đo và truyền dữ liệu, còn gateway hoạt động liên tục để nhận dữ liệu và phục vụ web/cloud.

---

## B4.2. Thiết Kế Phần Mềm Node Cảm Biến

Firmware node được triển khai chủ yếu trong `src/node_main.cpp`. Do node dùng deep sleep, hầu hết công việc được thực hiện trong hàm `setup()`, còn `loop()` gần như không phải vòng lặp chính như các ứng dụng Arduino thông thường.

### B4.2.1. Chức năng chính của node

Node đảm nhiệm các chức năng sau:

- Điều khiển nguồn cấp cho cảm biến qua `SensorPowerPin = GPIO32`.
- Đọc cảm biến DHT22 để lấy nhiệt độ và độ ẩm không khí.
- Đọc cảm biến độ ẩm đất điện dung qua ADC.
- Đọc điện áp pin qua mạch chia áp.
- Đóng gói dữ liệu telemetry.
- Gửi dữ liệu qua LoRa.
- Chờ ACK từ gateway.
- Nhận và xử lý command cấu hình từ gateway.
- Lưu cấu hình runtime vào NVS bằng `Preferences`.
- Tính chu kỳ sleep cố định hoặc adaptive.
- Đưa ESP32 vào deep sleep.

### B4.2.2. Runtime config của node

Node có cấu hình runtime được lưu trong NVS namespace `node_cfg`. Các thông số này có thể được thay đổi từ gateway thông qua command LoRa.

Các thông số runtime gồm:

| Thông số | Ý nghĩa |
| --- | --- |
| `soilThresholdVol` | Ngưỡng độ ẩm đất cấu hình từ gateway |
| `sleepMinutes` | Chu kỳ ngủ cố định, đơn vị phút |
| `filterMode` | Chế độ lọc cấu hình: `0=AVERAGE`, `1=MEDIAN` |
| `pumpSeconds` | Thời gian bơm cấu hình |
| `controlMode` | Chế độ điều khiển: `0=MANUAL`, `1=AUTO` |
| `dutyCycleMode` | Chế độ chu kỳ: `0=FIXED`, `1=ADAPTIVE` |

Lưu ý: một số thông số hiện được lưu và gửi kèm telemetry để phục vụ dashboard/báo cáo, nhưng chưa tác động hoàn toàn vào thuật toán điều khiển bên dưới. Ví dụ driver soil hiện vẫn lọc median cố định, còn phân loại đất vẫn dùng các ngưỡng cố định trong code.

### B4.2.3. Luồng hoạt động của node

Luồng hoạt động chính:

```text
Start / Wake from deep sleep
        |
        v
Serial init
        |
        v
Enable sensor power GPIO32
        |
        v
Load runtime config from NVS
        |
        v
Init DHT22 + Soil sensor + LoRa
        |
        v
Read DHT22, soil, battery
        |
        v
Create DATA packet + CRC
        |
        v
Send packet via LoRa
        |
        v
Wait ACK from gateway
        |
        +---- ACK OK + Command? ----> Execute command + save config
        |
        v
Compute sleep duration
        |
        v
Turn off LoRa/SPI/sensor power
        |
        v
Deep sleep
```

Nếu LoRa khởi tạo thất bại, node không tiếp tục đọc/gửi dữ liệu mà chuyển sang deep sleep theo chu kỳ hiện tại để tiết kiệm năng lượng.

### B4.2.4. Quản lý năng lượng

Node sử dụng deep sleep để giảm tiêu thụ năng lượng. Trước khi ngủ, node:

- Đưa LoRa vào sleep và gọi `LoRa.end()`.
- Dừng SPI bằng `SPI.end()`.
- Kéo chân cấp nguồn cảm biến `GPIO32` xuống LOW.
- Gọi `esp_deep_sleep_start()`.

Node hỗ trợ hai chế độ chu kỳ:

| Chế độ | Ý nghĩa |
| --- | --- |
| `FIXED` | Ngủ theo `runtimeConfig.sleepMinutes` |
| `ADAPTIVE` | Tự điều chỉnh thời gian ngủ theo pin và trạng thái đất |

Thuật toán adaptive sleep hiện tại:

| Điều kiện | Sleep |
| --- | ---: |
| Lỗi cảm biến | 30 phút |
| Pin `< 3.3V` | 90 phút |
| Pin `< 3.5V` | 60 phút |
| Đất `URGENT_WATERING` | 5 phút |
| Đất `NEED_WATERING` | 10 phút |
| Đất `LIGHT_DRY` | 20 phút |
| Đất `OVER_MOISTURE` | 60 phút |
| Bình thường | 30 phút |

Thiết kế này giúp node gửi dữ liệu thường xuyên hơn khi đất khô cần tưới, và giảm tần suất gửi khi pin yếu hoặc đất quá ẩm.

---

## B4.3. Thiết Kế Driver Cảm Biến

### B4.3.1. Driver DHT22

Driver DHT22 được triển khai trong `src/dht22_sensor.cpp/.h`. Driver không dùng thư viện ngoài mà tự điều khiển chân GPIO theo timing single-wire của DHT22.

Quy trình đọc DHT22:

1. ESP32 kéo chân DATA xuống LOW khoảng 20 ms để gửi tín hiệu start.
2. ESP32 nhả chân DATA lên HIGH khoảng 40 us.
3. Chuyển chân DATA sang `INPUT_PULLUP`.
4. Chờ phản hồi LOW/HIGH từ DHT22.
5. Đọc 40 bit dữ liệu thành 5 byte.
6. Kiểm tra checksum:

```text
checksum = (byte0 + byte1 + byte2 + byte3) & 0xFF
```

7. Tính nhiệt độ và độ ẩm:

```text
H_air = rawHumidity / 10.0 + HumidityOffsetRh
T_air = rawTemperature / 10.0 + TempOffsetC
```

8. Validate giá trị trong dải hợp lệ.
9. Loại mẫu nhảy bất thường so với mẫu trước.
10. Lưu vào buffer trung bình trượt 5 mẫu.
11. Trả kết quả làm tròn 0.1.

Các điều kiện lỗi được đánh dấu bằng `errorFlag`, gồm lỗi timing, lỗi checksum hoặc giá trị nằm ngoài dải hợp lệ.

### B4.3.2. Driver cảm biến độ ẩm đất

Driver cảm biến độ ẩm đất được triển khai trong `src/soil_moisture.cpp/.h`. Cảm biến được đọc qua chân ADC `GPIO34`.

Quy trình đọc:

1. Cấu hình ADC 12 bit.
2. Cấu hình attenuation `ADC_11db`.
3. Đọc 11 mẫu ADC.
4. Bỏ 3 mẫu đầu để tránh nhiễu lúc ADC/cảm biến vừa ổn định.
5. Kiểm tra ADC có nằm trong dải hợp lệ `100..4090`.
6. Nếu ADC không hợp lệ, retry tối đa 3 lần.
7. Lọc median trên các mẫu còn lại.
8. Quy đổi ADC sang `%Vol` theo calibration.

Công thức quy đổi:

```text
H_soil = (ADC_dry - ADC_filtered) * 60 / (ADC_dry - ADC_wet)
```

Với calibration hiện tại:

| Mốc calibration | ADC |
| --- | ---: |
| Đất khô `ADC_dry` | 3400 |
| Đất ướt `ADC_wet` | 1200 |

Kết quả được giới hạn trong `0..60 %Vol`.

Phân loại trạng thái đất:

| Độ ẩm đất | Trạng thái |
| ---: | --- |
| `> 42 %Vol` | `OVER_MOISTURE` |
| `> 33..42 %Vol` | `NORMAL` |
| `> 24..33 %Vol` | `LIGHT_DRY` |
| `> 15..24 %Vol` | `NEED_WATERING` |
| `<= 15 %Vol` | `URGENT_WATERING` |

### B4.3.3. Đo điện áp pin

Node đo điện áp pin qua chân ADC `GPIO35` và mạch chia áp `220k/100k`. Code dùng `analogReadMilliVolts()` để lấy điện áp tại chân ADC, sau đó nhân với hệ số chia áp.

```text
V_bat = V_gpio35 * BatteryDividerRatio
```

Với cấu hình hiện tại:

```text
BatteryDividerRatio = 3.2
```

Node đọc 16 mẫu, lấy trung bình để giảm nhiễu.

---

## B4.4. Thiết Kế Giao Thức Truyền Thông LoRa

Giao thức truyền LoRa được triển khai trong `src/packet_protocol.cpp/.h`. Hệ thống dùng packet dạng text `KEY=VALUE`, dễ debug qua Serial Monitor và dễ mở rộng trường dữ liệu.

### B4.4.1. Các loại packet

Các packet chính:

| Packet | Chiều truyền | Chức năng |
| --- | --- | --- |
| `DATA` | Node → Gateway | Gửi telemetry cảm biến |
| `ACK` | Gateway → Node | Xác nhận nhận dữ liệu, có thể kèm command |
| `OTA_CHUNK` | Gateway → Node | Gửi chunk OTA mô phỏng qua LoRa |
| `OTA_STATUS` | Node → Gateway | Báo trạng thái nhận chunk OTA mô phỏng |

### B4.4.2. Gói DATA

Gói `DATA` chứa dữ liệu cảm biến và cấu hình hiện tại của node.

Ví dụ:

```text
TYPE=DATA,NODE=1,PID=1,T=30.1,HA=70.2,HS=25.0,VB=4.05,ADC=2480,SOIL=LIGHT_DRY,ERR=0,CSLEEP=30,CTH=20,CFILTER=0,CPUMP=5,CMODE=1,CDUTY=1,CRC=....
```

Ý nghĩa các trường:

| Trường | Ý nghĩa |
| --- | --- |
| `TYPE=DATA` | Loại gói telemetry |
| `NODE` | ID node |
| `PID` | Packet ID, tăng sau mỗi chu kỳ gửi |
| `T` | Nhiệt độ không khí |
| `HA` | Độ ẩm không khí |
| `HS` | Độ ẩm đất `%Vol` |
| `VB` | Điện áp pin |
| `ADC` | ADC đất sau lọc |
| `SOIL` | Trạng thái đất |
| `ERR` | Cờ lỗi cảm biến |
| `CSLEEP` | Sleep config hiện tại |
| `CTH` | Soil threshold config |
| `CFILTER` | Filter mode config |
| `CPUMP` | Pump seconds config |
| `CMODE` | Control mode config |
| `CDUTY` | Duty cycle mode config |
| `CRC` | CRC16-CCITT |

### B4.4.3. Gói ACK và command

Gateway trả ACK cho node sau khi nhận `DATA`.

Ví dụ:

```text
TYPE=ACK,NODE=1,PID=1,OK=1,CMD=NONE,PARAM=0,STATUS=OK,CRC=....
```

Nếu gateway có command đang chờ gửi cho node, ACK sẽ chứa command đó.

Các command đã triển khai:

| Command | Chức năng |
| --- | --- |
| `SET_SLEEP_DURATION` | Đổi thời gian ngủ cố định của node |
| `SET_THRESHOLD` | Lưu ngưỡng độ ẩm đất cấu hình |
| `SET_FILTER_MODE` | Lưu chế độ lọc cấu hình |
| `SET_PUMP_TIME` | Lưu thời gian bơm cấu hình |
| `SET_CONTROL_MODE` | Lưu chế độ manual/auto |
| `SET_DUTY_CYCLE` | Chọn fixed/adaptive sleep |
| `SLEEP_NOW` | Command sleep/log |
| `START_PUMP` | Bật output bơm legacy/test trong thời gian chỉ định |
| `START_OTA` | Bắt đầu OTA mô phỏng qua LoRa |
| `RESEND_CHUNK` | Có trong enum/protocol, chưa có luồng xử lý chính |

### B4.4.4. CRC16-CCITT

Mỗi packet đều có trường `CRC` ở cuối để kiểm tra lỗi truyền. Hàm encode tạo payload, chuyển payload sang uppercase trước khi tính CRC, sau đó gắn CRC vào cuối gói.

Gateway và node đều gọi decode tương ứng để kiểm tra:

```text
hasValidCrc(packet) == true
```

Nếu CRC sai, gói bị bỏ qua hoặc trả trạng thái lỗi tùy loại packet.

---

## B4.5. Thiết Kế Phần Mềm Gateway

Gateway được chia thành nhiều module nhỏ để tách phần LoRa, web, cloud, state và logic nghiệp vụ.

### B4.5.1. Entry point gateway

`src/gateway_main.cpp` là entry point của gateway. File này có nhiệm vụ:

- Khởi tạo Serial.
- Khởi tạo LoRa.
- Kết nối WiFi.
- Khởi động HTTP server.
- Khởi động task web server.
- Chạy vòng lặp nhận gói LoRa.

Gateway hoạt động liên tục, không deep sleep.

### B4.5.2. Gateway state

`src/gateway_state.cpp/.h` chứa các biến trạng thái dùng chung:

- Dữ liệu telemetry mới nhất.
- Vòng đệm history trong RAM.
- Pending command cho từng node.
- Số lần lặp lại command.
- Trạng thái LoRa.
- Trạng thái OTA mô phỏng.
- Mutex/critical section cho command queue.

History dùng vòng đệm RAM, nên dữ liệu sẽ mất khi gateway reset. Thiết kế này đủ cho dashboard realtime và export CSV trong phiên chạy hiện tại.

### B4.5.3. Gateway logic

`src/gateway_logic.cpp/.h` chứa các hàm xử lý nghiệp vụ:

- Parse tham số command từ HTTP.
- Queue command cho node.
- Tính alert theo độ ẩm đất.
- Tính thời gian bơm khuyến nghị.
- Lưu telemetry vào history.
- Quản lý pending command và retry.

Gateway chỉ tự gửi command bơm khi `Config::EnablePumpHardware = true`. Hiện tại cấu hình này đang tắt, nên bơm thật không được kích hoạt tự động.

### B4.5.4. Gateway LoRa

`src/gateway_lora.cpp/.h` đảm nhiệm giao tiếp LoRa phía gateway.

Quy trình xử lý packet:

1. Gọi `LoRa.parsePacket()` để kiểm tra có gói mới.
2. Đọc chuỗi packet từ LoRa.
3. Nếu là `OTA_STATUS`, xử lý trạng thái OTA mô phỏng.
4. Nếu là `DATA`, decode telemetry và kiểm tra CRC.
5. Kiểm tra gói trùng theo `NODE/PID`.
6. Nếu gói mới, lưu telemetry vào history.
7. In log RSSI/SNR/alert/pump time.
8. Upload ThingsBoard nếu cloud sẵn sàng.
9. Gửi ACK về node.

Gateway vẫn gửi lại ACK khi nhận packet trùng để node có thể nhận được ACK nếu ACK trước đó bị mất.

### B4.5.5. Retry command

Command từ gateway gửi xuống node được gắn vào ACK. Vì node chỉ thức ngắn trong mỗi chu kỳ, gateway không gửi command trực tiếp bất kỳ lúc nào, mà lưu command vào hàng đợi.

Khi node gửi telemetry tiếp theo, gateway gắn pending command vào ACK. Command được lặp lại một số lần để tăng xác suất node nhận được.

Riêng `START_OTA` được lặp nhiều hơn command thông thường vì quá trình OTA mô phỏng cần node chắc chắn nhận được lệnh bắt đầu.

---

## B4.6. Thiết Kế Web Dashboard Và Cloud

### B4.6.1. Web dashboard

`src/gateway_web.cpp/.h` triển khai WiFi và HTTP server cho gateway.

Gateway mở HTTP server tại port `80` và debug server đơn giản tại port `8080`.

Các endpoint chính:

| Endpoint | Chức năng |
| --- | --- |
| `/` | Dashboard HTML hiển thị telemetry/history/config node |
| `/ping` | Trả `pong`, dùng kiểm tra gateway còn online |
| `/export.csv` | Xuất history telemetry dạng CSV |
| `/command?node=1&cmd=...&param=...` | Queue command gửi cho node |
| `/self_ota?url=...&md5=...` | Gateway self-OTA qua WiFi/HTTP |

Dashboard hiển thị các thông tin:

- Thời gian nhận packet.
- Node ID.
- Packet ID.
- Nhiệt độ.
- Độ ẩm không khí.
- Độ ẩm đất.
- Điện áp pin.
- Trạng thái/cảnh báo đất.
- RSSI.
- Cấu hình sleep, threshold, pump, control mode, duty mode.

### B4.6.2. Export CSV

Endpoint `/export.csv` trả về dữ liệu history trong RAM dưới dạng CSV. Chức năng này phục vụ phân tích dữ liệu và đưa số liệu vào báo cáo.

Các cột CSV gồm:

```text
time_ms,node_id,packet_id,t_air,h_air,h_soil,v_bat,adc,soil_status,error_flag,rssi,snr,sleep_min,soil_threshold,filter_mode,pump_time,control_mode,duty_cycle
```

### B4.6.3. Command endpoint

Gateway nhận command từ URL:

```text
/command?node=1&cmd=SET_SLEEP_DURATION&param=10
```

Quy trình:

1. Parse `node`.
2. Parse `cmd` thành enum `CommandType`.
3. Parse `param` theo loại command.
4. Kiểm tra hợp lệ.
5. Queue command vào pending command.
6. Trả response `queued`.

Command không được gửi ngay lập tức, mà sẽ được gắn vào ACK ở lần node gửi telemetry tiếp theo.

### B4.6.4. Upload ThingsBoard

`src/gateway_cloud.cpp/.h` phụ trách upload telemetry lên ThingsBoard bằng HTTP POST.

Gateway chỉ upload khi:

- WiFi đã kết nối.
- Cloud upload được bật trong cấu hình.
- Có telemetry hợp lệ.

Dữ liệu gửi lên ThingsBoard gồm các thông số môi trường và trạng thái node/gateway. Thiết kế này cho phép theo dõi dữ liệu từ xa ngoài dashboard nội bộ.

---

## B4.7. Các Module Phần Mềm Đã Triển Khai

Phần mềm của hệ thống được tổ chức theo từng module riêng trong thư mục `src/`, tách biệt giữa firmware node cảm biến, gateway, driver cảm biến, giao thức LoRa và các chức năng web/cloud.

| Module | Chức năng |
| --- | --- |
| `src/project_config.h` | Cấu hình GPIO, LoRa, WiFi, ThingsBoard, sleep, calibration, OTA version và các cờ bật/tắt chức năng |
| `src/node_main.cpp` | Firmware node: đọc cảm biến, đo pin, gửi LoRa telemetry, chờ ACK, nhận command, lưu NVS, deep sleep và OTA mô phỏng qua LoRa |
| `src/gateway_main.cpp` | Entry point gateway: khởi tạo Serial, LoRa, WiFi/web/cloud và chạy vòng lặp xử lý LoRa |
| `src/gateway_state.cpp/.h` | Quản lý trạng thái gateway, telemetry history, pending command, trạng thái LoRa và OTA mô phỏng |
| `src/gateway_logic.cpp/.h` | Logic nghiệp vụ gateway: parse command, tính alert, tính pump time, lưu telemetry và queue command |
| `src/gateway_lora.cpp/.h` | Giao tiếp LoRa phía gateway: nhận DATA, kiểm CRC, chống gói trùng, gửi ACK/command, retry và OTA mô phỏng |
| `src/dht22_sensor.cpp/.h` | Driver DHT22 tự viết bằng timing single-wire, checksum, validate và trung bình trượt |
| `src/soil_moisture.cpp/.h` | Driver soil sensor: đọc ADC, retry, lọc median, quy đổi `%Vol` và phân loại trạng thái đất |
| `src/packet_protocol.cpp/.h` | Encode/decode packet `DATA`, `ACK`, `OTA_CHUNK`, `OTA_STATUS`, command enum và CRC16-CCITT |
| `src/gateway_web.cpp/.h` | Web dashboard, export CSV, command endpoint, WiFi setup và gateway self-OTA |
| `src/gateway_cloud.cpp/.h` | Upload telemetry lên ThingsBoard bằng HTTP POST |
| `platformio.ini` | Cấu hình build PlatformIO cho environment `node` và `gateway`; gateway dùng partition table OTA |
| `partitions_gateway_ota.csv` | Partition table cho gateway OTA thật, gồm hai vùng app `ota_0/ota_1` và vùng `otadata` |

---

## B4.8. Thiết Kế OTA

Hệ thống hiện có hai hướng OTA khác nhau: gateway self-OTA thật qua WiFi/HTTP và OTA mô phỏng cho node qua LoRa.

### B4.8.1. Gateway self-OTA qua WiFi/HTTP

Gateway self-OTA được triển khai trong `src/gateway_web.cpp`, tại endpoint:

```text
/self_ota?url=http://server/firmware.bin&md5=optional_md5
```

Quy trình gateway OTA:

1. Người dùng truy cập endpoint `/self_ota` và truyền URL file firmware `.bin`.
2. Gateway kiểm tra WiFi đã kết nối.
3. Gateway dùng `HTTPClient` tải firmware từ URL.
4. Gateway gọi `Update.begin(contentLength)` để bắt đầu ghi OTA.
5. Nếu có MD5, gateway gọi `Update.setMD5()` để kiểm tra firmware.
6. Gateway ghi firmware bằng `Update.writeStream()`.
7. Gateway gọi `Update.end(true)` để hoàn tất.
8. Nếu thành công, gateway in log `OTA_SUCCESS` và restart.
9. Sau restart, ESP32 boot vào phân vùng firmware mới.

Đây là OTA thật vì firmware mới được ghi vào flash của gateway.

### B4.8.2. Partition table cho gateway OTA

Gateway dùng file `partitions_gateway_ota.csv`, được khai báo trong `platformio.ini`:

```ini
board_build.partitions = partitions_gateway_ota.csv
```

Nội dung partition table:

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x140000,
app1,     app,  ota_1,   0x150000, 0x140000,
spiffs,   data, spiffs,  0x290000, 0x170000,
```

Ý nghĩa:

| Phân vùng | Vai trò |
| --- | --- |
| `nvs` | Lưu dữ liệu cấu hình nhỏ |
| `otadata` | Lưu thông tin phân vùng OTA đang được boot |
| `app0 / ota_0` | Vùng firmware ứng dụng thứ nhất |
| `app1 / ota_1` | Vùng firmware ứng dụng thứ hai |
| `spiffs` | Vùng filesystem SPIFFS |

Cơ chế OTA cần hai phân vùng app. Khi gateway đang chạy ở `ota_0`, firmware mới sẽ được ghi vào `ota_1`. Sau khi ghi thành công, ESP32 cập nhật `otadata` để lần boot tiếp theo chạy từ `ota_1`. Lần OTA sau có thể ghi ngược lại vào `ota_0`.

### B4.8.3. OTA mô phỏng cho node qua LoRa

Node OTA qua LoRa hiện mới ở mức mô phỏng, chưa ghi firmware thật vào flash.

Các module liên quan:

| File | Vai trò trong OTA LoRa mô phỏng |
| --- | --- |
| `packet_protocol.cpp/.h` | Định nghĩa `START_OTA`, `OTA_CHUNK`, `OTA_STATUS`, CRC |
| `gateway_lora.cpp/.h` | Gateway gửi các chunk firmware mô phỏng qua LoRa |
| `node_main.cpp` | Node nhận chunk, kiểm CRC và trả trạng thái |

Luồng OTA mô phỏng:

1. Người dùng queue command `START_OTA` từ web dashboard.
2. Gateway gắn command `START_OTA` vào ACK gửi cho node.
3. Node nhận command và gọi hàm OTA mô phỏng.
4. Node liên tục gửi `OTA_READY` để báo sẵn sàng nhận chunk.
5. Gateway nhận `OTA_READY` và bắt đầu gửi các gói `OTA_CHUNK`.
6. Node kiểm tra CRC từng chunk.
7. Nếu chunk đúng, node trả `OTA_STATUS` với `ACK`.
8. Nếu chunk sai CRC, node trả `NACK_CRC`.
9. Sau khi nhận đủ chunk, node gửi `OTA_SUCCESS`.

Điểm quan trọng: trong code node có log:

```text
OTA simulation only; firmware flash is not modified
```

Điều này cho thấy OTA LoRa hiện tại chỉ chứng minh được quy trình truyền chunk, ACK/NACK và kiểm CRC, chưa thực hiện cập nhật firmware thật cho node.

---

## B4.9. Thiết Kế Độ Tin Cậy Và Xử Lý Lỗi

Hệ thống có một số cơ chế tăng độ tin cậy:

### B4.9.1. CRC packet

Tất cả packet LoRa quan trọng đều có CRC16-CCITT. Gateway và node chỉ xử lý packet nếu CRC hợp lệ.

### B4.9.2. ACK cho telemetry

Sau khi node gửi telemetry, node chờ ACK trong thời gian `AckTimeoutMs = 2500 ms`. Nếu không nhận ACK, node vẫn tiếp tục đi ngủ để tiết kiệm năng lượng.

### B4.9.3. Chống packet trùng

Gateway lưu `NODE/PID` gần nhất. Nếu nhận lại packet trùng, gateway không lưu telemetry lần nữa nhưng vẫn gửi lại ACK để node có thể hoàn tất chu kỳ.

### B4.9.4. Retry command

Command gateway gửi cho node được lặp lại trong nhiều ACK để tăng xác suất node nhận được, do truyền thông LoRa có thể bị mất gói.

### B4.9.5. Validate cảm biến

DHT22 và soil sensor đều có kiểm tra lỗi:

- DHT22 kiểm checksum, dải nhiệt độ/độ ẩm và mẫu nhảy bất thường.
- Soil sensor kiểm ADC nằm trong dải hợp lệ và retry nếu đọc lỗi.

Nếu cảm biến lỗi, packet telemetry có `ERR=1`, gateway vẫn ACK nhưng trạng thái ACK có thể là `SENSOR_ERROR`, và node sẽ bỏ qua command khi ACK không OK.

---

## B4.10. Thiết Kế Khả Năng Mở Rộng

Thiết kế hiện tại có thể mở rộng theo các hướng sau:

### B4.10.1. Mở rộng nhiều node

Protocol đã có trường `NODE`, gateway có pending command theo node ID. Vì vậy có thể mở rộng thêm nhiều node bằng cách cấp ID khác nhau cho từng node.

### B4.10.2. Thêm loại cảm biến mới

Có thể thêm driver cảm biến mới dưới dạng module riêng, sau đó bổ sung trường dữ liệu vào `TelemetryPacket` và hàm encode/decode trong `packet_protocol.cpp`.

### B4.10.3. Lưu history bền vững

Hiện history nằm trong RAM, mất khi gateway reset. Có thể mở rộng lưu vào SPIFFS, LittleFS, SD card hoặc gửi toàn bộ lên cloud.

### B4.10.4. OTA node thật qua LoRa

OTA node hiện là mô phỏng. Để thành OTA thật cần bổ sung:

- Partition table OTA cho node.
- Cơ chế ghi firmware vào phân vùng OTA bằng `Update.h` hoặc API OTA phù hợp.
- Chia firmware `.bin` thành chunk nhỏ phù hợp LoRa.
- Kiểm CRC từng chunk và CRC toàn firmware.
- Cơ chế resume/retry chunk bị mất.
- Xác thực firmware trước khi boot.

### B4.10.5. Điều khiển bơm thật

Code hiện có `PumpPin = GPIO25` và command `START_PUMP`, nhưng `EnablePumpHardware = false`. Nếu triển khai bơm thật, cần bổ sung phần cứng driver MOSFET/transistor, diode bảo vệ, nguồn riêng và logic an toàn chống bơm quá lâu.

---

## B4.11. Kết Luận Thiết Kế Phần Mềm

Phần mềm hệ thống được thiết kế theo hướng module hóa, tách rõ firmware node và gateway. Node tập trung vào đo đạc, truyền dữ liệu và tiết kiệm năng lượng bằng deep sleep. Gateway tập trung vào nhận dữ liệu, xử lý logic, hiển thị web, gửi cloud và quản lý command.

Giao thức LoRa dạng text có CRC giúp dễ debug và đủ tin cậy cho mô hình đồ án. Web dashboard và export CSV giúp quan sát dữ liệu trực tiếp, trong khi ThingsBoard hỗ trợ giám sát từ xa. Cơ chế OTA gateway đã được triển khai thật thông qua WiFi/HTTP và partition table OTA, còn OTA node qua LoRa hiện ở mức mô phỏng để chứng minh quy trình truyền chunk và xác nhận trạng thái.
