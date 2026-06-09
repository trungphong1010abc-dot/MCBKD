# Project Context - EE4552 Wireless Sensor Network

Tài liệu này mô tả phần mềm theo code hiện tại trong `src/`. Đọc cùng `README/HARDWARE_WIRING_GUIDE.md` để nắm cả phần cứng và luồng hoạt động.

## 1. Mục Tiêu Dự Án

Hệ thống là mạng cảm biến không dây dùng ESP32 và LoRa để giám sát môi trường đất/không khí.

- `node`: đọc DHT22, cảm biến độ ẩm đất điện dung, điện áp pin; gửi telemetry qua LoRa; nhận ACK/command; lưu cấu hình runtime vào NVS; sau đó deep sleep.
- `gateway`: nhận telemetry LoRa, lưu history RAM, phục vụ dashboard web, xuất CSV, gửi telemetry lên ThingsBoard, queue command cho node và hỗ trợ gateway self-OTA qua WiFi.

Hiện tại bơm thật không được bật tự động vì `Config::EnablePumpHardware = false`. Code vẫn có `PumpPin = GPIO25` và command `START_PUMP` cho mục đích legacy/test, nhưng tài liệu phần cứng không khuyến nghị đấu bơm trực tiếp.

## 2. Cấu Trúc Code

| File | Vai trò |
| --- | --- |
| `src/project_config.h` | Cấu hình GPIO, LoRa, WiFi, ThingsBoard, sleep, calibration, OTA version |
| `src/node_main.cpp` | Firmware node: cảm biến, pin, LoRa telemetry, ACK/command, NVS, deep sleep, OTA mô phỏng |
| `src/gateway_main.cpp` | Entry point gateway |
| `src/gateway_state.cpp/.h` | Biến trạng thái gateway, history, pending command, OTA state |
| `src/gateway_logic.cpp/.h` | Parse command, tính alert, pump time, lưu telemetry, queue command |
| `src/gateway_lora.cpp/.h` | Init/process LoRa, ACK, chống packet trùng, OTA mô phỏng |
| `src/gateway_cloud.cpp/.h` | Upload ThingsBoard qua HTTP |
| `src/gateway_web.cpp/.h` | WiFi, dashboard HTTP, CSV, command endpoint, gateway self-OTA |
| `src/dht22_sensor.cpp/.h` | Driver DHT22 tự viết bằng timing |
| `src/soil_moisture.cpp/.h` | Đọc ADC đất, median filter, tính `%Vol`, phân loại trạng thái |
| `src/packet_protocol.cpp/.h` | Packet text, command enum, CRC16-CCITT, encode/decode telemetry/ACK/OTA |
| `platformio.ini` | Build env `node` và `gateway`; default env hiện là `gateway` |

Build:

```text
pio run -e node
pio run -e gateway
```

## 3. Cấu Hình Chính

| Nhóm | Giá trị hiện tại |
| --- | --- |
| Serial | `115200` |
| ID | `NodeId = 1`, `GatewayId = 1` |
| DHT22 | `GPIO27`, offset nhiệt/ẩm đều `0` |
| Soil ADC | `GPIO34`, dry `3400`, wet `1200`, valid ADC `100..4090` |
| Battery ADC | `GPIO35`, `BatteryDividerRatio = 3.2` |
| Sensor power | `GPIO32`, HIGH khi node thức, LOW trước sleep |
| Pump | `GPIO25`, active HIGH, max `15s`, `EnablePumpHardware=false` |
| LoRa | `433E6`, SF7, BW125kHz, CR4/5, TX power 17dBm, CRC bật |
| ACK timeout | `2500ms` |
| Deep sleep | Bật; default `30` phút; adaptive `5..90` phút |
| Gateway HTTP | Dashboard port `80`, debug ping server port `8080` |
| ThingsBoard | HTTP port `80`, upload bật nếu WiFi kết nối |
| Gateway firmware version | `GW_OTA_TEST_1` |

## 4. Luồng Hoạt Động Của Node

Node chạy chủ yếu trong `setup()` vì deep sleep được bật.

1. Mở Serial.
2. Kéo `SensorPowerPin = GPIO32` lên HIGH.
3. Cấu hình `PumpPin = GPIO25` và tắt bơm/output.
4. Đọc runtime config từ Preferences namespace `node_cfg`.
5. Khởi tạo DHT22 tại `GPIO27`.
6. Khởi tạo soil sensor tại `GPIO34`.
7. Reset/init LoRa. Nếu fail, node ngủ theo `runtimeConfig.sleepMinutes` hoặc default 30 phút.
8. Đọc DHT22, đất và pin.
9. Tạo `TelemetryPacket`, kèm cả dữ liệu cảm biến và config hiện tại của node.
10. Gửi packet `TYPE=DATA` qua LoRa, chờ ACK tối đa `2500ms`.
11. Nếu ACK hợp lệ, `OK=1` và có command, node thực thi command rồi lưu config nếu cần.
12. Tính thời gian sleep fixed/adaptive.
13. Cho LoRa sleep/end, dừng SPI, kéo `GPIO32` LOW và vào deep sleep.

Nếu `Config::EnableDeepSleep=false`, `loop()` sẽ delay theo `rtcSleepMinutes` rồi restart.

## 5. Driver DHT22

Driver trong `dht22_sensor.cpp`:

1. ESP32 kéo DATA LOW khoảng `20ms`, nhả HIGH `40us`, sau đó chuyển `INPUT_PULLUP`.
2. Đợi response LOW/HIGH từ DHT22.
3. Đọc 40 bit thành 5 byte.
4. Check checksum: `(B0 + B1 + B2 + B3) & 0xFF == B4`.
5. Tính:

```text
H_air = rawHumidity / 10.0 + HumidityOffsetRh
T_air = rawTemperature / 10.0 + TempOffsetC
```

6. Validate `-10..50 C`, `0..100 %RH`, và loại mẫu nhảy quá `2 C` hoặc `5 %RH` so với mẫu trước.
7. Lưu vào buffer trung bình trượt 5 mẫu, trả giá trị làm tròn 0.1.
8. Nếu lỗi timing/checksum/validate, trả `errorFlag = 1`.

## 6. Driver Độ Ẩm Đất

Driver trong `soil_moisture.cpp`:

1. ADC 12 bit, attenuation `ADC_11db`.
2. Đọc 11 mẫu từ `GPIO34`.
3. Bỏ 3 mẫu đầu.
4. Nếu ADC nằm ngoài `100..4090`, retry sau `350ms`; lỗi 3 lần thì `errorFlag = 1`.
5. Lọc median các mẫu còn lại.
6. Quy đổi:

```text
H_soil = (3400 - ADC_filtered) * 60 / (3400 - 1200)
```

7. Giới hạn `0..60 %Vol`, làm tròn 0.1.

Phân loại:

| H_soil | Trạng thái |
| ---: | --- |
| `> 42` | `OVER_MOISTURE` |
| `> 33..42` | `NORMAL` |
| `> 24..33` | `LIGHT_DRY` |
| `> 15..24` | `NEED_WATERING` |
| `<= 15` | `URGENT_WATERING` |

## 7. Đo Điện Áp Pin

Node đọc pin trong `readBatteryVoltage()`:

1. ADC 12 bit, attenuation `ADC_11db` tại `GPIO35`.
2. Đọc 16 mẫu `analogReadMilliVolts()`.
3. Lấy trung bình điện áp tại chân ADC.
4. Nhân `BatteryDividerRatio = 3.2`.

```text
V_bat = V_gpio35 * 3.2
```

Pin ảnh hưởng adaptive sleep:

| Điều kiện | Sleep |
| --- | ---: |
| `V_bat < 3.3V` | 90 phút |
| `V_bat < 3.5V` | 60 phút |

## 8. Packet LoRa Và CRC

Protocol dùng chuỗi text `KEY=VALUE` và CRC16-CCITT ở cuối. Hàm encode chuyển payload thành uppercase trước khi tính và gắn CRC.

Telemetry node gửi gateway:

```text
TYPE=DATA,NODE=1,PID=1,T=30.1,HA=70.2,HS=25,VB=4.05,ADC=2480,SOIL=LIGHT_DRY,ERR=0,CSLEEP=30,CTH=20,CFILTER=0,CPUMP=5,CMODE=1,CDUTY=1,CRC=....
```

| Trường | Ý nghĩa |
| --- | --- |
| `TYPE=DATA` | Telemetry |
| `NODE` | ID node |
| `PID` | Packet ID, lưu trong RTC và tăng mỗi chu kỳ |
| `T` | Nhiệt độ không khí |
| `HA` | Độ ẩm không khí |
| `HS` | Độ ẩm đất `%Vol` |
| `VB` | Điện áp pin |
| `ADC` | ADC đất sau lọc |
| `SOIL` | Trạng thái đất do node phân loại |
| `ERR` | Lỗi cảm biến |
| `CSLEEP` | Sleep config hiện tại của node |
| `CTH` | Soil threshold config |
| `CFILTER` | Filter mode config: `0=AVERAGE`, `1=MEDIAN` |
| `CPUMP` | Pump seconds config |
| `CMODE` | Control mode config: `0=MANUAL`, `1=AUTO` |
| `CDUTY` | Duty mode config: `0=FIXED`, `1=ADAPTIVE` |
| `CRC` | CRC16 của payload trước trường CRC |

ACK gateway gửi node:

```text
TYPE=ACK,NODE=1,PID=1,OK=1,CMD=NONE,PARAM=0,STATUS=OK,CRC=....
```

Command hiện có:

| Command | Ý nghĩa |
| --- | --- |
| `SET_SLEEP_DURATION` | Đổi sleep fixed, nhận `5..90` phút; có hiệu lực khi duty mode là `FIXED` |
| `SET_THRESHOLD` | Lưu ngưỡng độ ẩm đất vào config và telemetry; phân loại đất hiện vẫn dùng ngưỡng cố định trong code |
| `SET_FILTER_MODE` | Lưu `AVERAGE` hoặc `MEDIAN`; driver soil hiện vẫn lọc median cố định |
| `SET_PUMP_TIME` | Lưu pump seconds config, tối đa 15s; `computePumpTime()` hiện vẫn tính theo thuật toán riêng |
| `SET_CONTROL_MODE` | Lưu `MANUAL` hoặc `AUTO`; chưa đổi luồng điều khiển chính |
| `SET_DUTY_CYCLE` | `FIXED` hoặc `ADAPTIVE` |
| `SLEEP_NOW` | Hiện chỉ log command |
| `START_PUMP` | Kéo `GPIO25` theo `PARAM` giây; legacy/test |
| `START_OTA` | Bắt đầu OTA mô phỏng qua LoRa |
| `RESEND_CHUNK` | Có trong enum/protocol, chưa có luồng xử lý chính |

## 9. Gateway LoRa Và ACK

Gateway `processLoRa()`:

1. Nếu nhận `TYPE=OTA_STATUS`, xử lý trạng thái OTA mô phỏng.
2. Nếu nhận `TYPE=DATA`, decode và check CRC.
3. Nếu `NODE/PID` trùng packet gần nhất, bỏ lưu history nhưng vẫn gửi lại ACK.
4. Lưu telemetry vào vòng đệm RAM 64 mẫu.
5. In log gồm RSSI, SNR, alert và `PumpTime`.
6. Upload ThingsBoard nếu WiFi/cloud sẵn sàng.
7. Gửi ACK về node.

ACK có `OK=0` nếu `packet.errorFlag != 0`, status `SENSOR_ERROR`. Node bỏ qua command khi ACK có `OK=0`. Pending command được gửi kèm ACK trong tối đa 3 lần, riêng `START_OTA` được lặp tối đa 10 lần để tăng xác suất node nhận được.

## 10. Gateway Web, CSV Và ThingsBoard

Gateway chạy `WebServer` port 80 trong task riêng pinned core 0. Ngoài ra có debug ping server đơn giản port 8080.

Endpoint:

| URL | Chức năng |
| --- | --- |
| `/` | Dashboard HTML, hiển thị history telemetry và config node |
| `/ping` | Trả `pong` |
| `/export.csv` | Xuất history CSV |
| `/command?node=1&cmd=SET_SLEEP_DURATION&param=10` | Queue command cho node |
| `/self_ota?url=http://server/firmware.bin&md5=optional` | Gateway self-OTA qua WiFi/HTTP |

CSV hiện có các cột:

```text
time_ms,node_id,packet_id,t_air,h_air,h_soil,v_bat,adc,soil_status,error_flag,rssi,snr,sleep_min,soil_threshold,filter_mode,pump_time,control_mode,duty_cycle
```

ThingsBoard HTTP telemetry:

```json
{
  "Node_ID": 1,
  "T_air": 30.1,
  "H_air": 70.2,
  "H_soil": 25,
  "V_bat": 4.05,
  "RSSI": -80,
  "Error_Flag": 0,
  "Alert": "LIGHT_DRY",
  "PumpTime": 3,
  "Sleep_Min": 30,
  "Soil_Threshold": 20,
  "Filter_Mode": 0,
  "Config_Pump_Time": 5,
  "Control_Mode": 1,
  "Duty_Cycle": 1
}
```

## 11. PumpTime Và Alert

Gateway tính `Alert` và `PumpTime` trong `gateway_logic.cpp`. Đây là giá trị gợi ý/mô phỏng, được log và upload cloud.

Nếu `errorFlag != 0`, `PumpTime = 0` và alert là `SENSOR_ERROR`.

PumpTime cơ sở:

| Điều kiện | PumpTime |
| --- | ---: |
| `H_soil > 42` | 0s |
| `33 < H_soil <= 42` | 0s |
| `24 < H_soil <= 33` | 3s |
| `15 < H_soil <= 24` | 6s |
| `H_soil <= 15` | 10s |

Hiệu chỉnh môi trường:

| Điều kiện | Điều chỉnh |
| --- | ---: |
| `T_air > 32 C` | `+2s` |
| `H_air < 50 %RH` | `+3s` |
| `H_air > 80 %RH` | `-2s` |

Kết quả được giới hạn `0..15s`. Gateway chỉ tự gửi `START_PUMP` nếu `EnablePumpHardware=true`; hiện tại cấu hình là `false`.

## 12. Adaptive Duty Cycle

Nếu `runtimeConfig.dutyCycleMode = FIXED`, node ngủ theo `runtimeConfig.sleepMinutes`.

Nếu `ADAPTIVE`, node dùng quy tắc:

| Điều kiện | Sleep |
| --- | ---: |
| `errorFlag != 0` | 30 phút |
| `V_bat < 3.3V` | 90 phút |
| `V_bat < 3.5V` | 60 phút |
| `SOIL=URGENT_WATERING` | 5 phút |
| `SOIL=NEED_WATERING` | 10 phút |
| `SOIL=LIGHT_DRY` | 20 phút |
| `SOIL=OVER_MOISTURE` | 60 phút |
| Còn lại / `NORMAL` | 30 phút |

Nếu `EnableFastUrgentSleepTest=true` và đất `URGENT_WATERING`, node có thể ngủ `FastUrgentSleepSeconds = 5` giây để test nhanh. Hiện flag này đang `false`.

## 13. Runtime Config Và Command

Gateway không gửi command riêng ngay lập tức. Command được queue trong `pendingCommands[nodeId]`. Khi node thức dậy và gửi telemetry, gateway chèn command vào ACK.

Node lưu config bằng ESP32 Preferences namespace `node_cfg`:

| Key | Ý nghĩa |
| --- | --- |
| `soil_th` | Ngưỡng độ ẩm đất |
| `sleep_min` | Sleep fixed |
| `filter` | Filter mode |
| `pump_s` | Pump seconds config |
| `ctrl` | Control mode |
| `duty` | Duty cycle mode |

Ví dụ:

```text
http://<gateway-ip>/command?node=1&cmd=SET_SLEEP_DURATION&param=10
http://<gateway-ip>/command?node=1&cmd=SET_DUTY_CYCLE&param=ADAPTIVE
http://<gateway-ip>/command?node=1&cmd=SET_FILTER_MODE&param=MEDIAN
http://<gateway-ip>/command?node=1&cmd=SET_THRESHOLD&param=20
```

Serial command trên gateway cũng được hỗ trợ:

```text
1 SET_SLEEP_DURATION 10
1 START_PUMP 5
```

## 14. OTA Trong Code

Có 2 cơ chế OTA/cập nhật:

### Gateway self-OTA thật qua WiFi

Endpoint:

```text
/self_ota?url=http://server/firmware.bin&md5=optional_md5
```

Gateway tải firmware qua HTTP, ghi bằng `Update`, thành công thì reboot. Partition OTA nằm trong `partitions_gateway_ota.csv`.

### Node OTA mô phỏng qua LoRa

Khi gateway queue `START_OTA`, node nhận command trong ACK và chạy phiên mô phỏng:

1. Node gửi `TYPE=OTA_STATUS,...,STATUS=OTA_READY`.
2. Gateway gửi 10 chunk giả lập `TYPE=OTA_CHUNK`.
3. Node kiểm tra CRC từng chunk.
4. Node trả `ACK`, `NACK_CRC`, `OTA_FAILED_TIMEOUT` hoặc `OTA_SUCCESS`.
5. Gateway retry mỗi chunk tối đa 3 lần.

Node không ghi firmware thật vào flash trong luồng này.

## 15. Debug Nhanh

Log node đúng:

```text
===== EE4552 NODE FIRMWARE =====
LoRa init OK
DHT: T=... C H=... %RH ERR=0
SOIL: ADC=... H=... %Vol status=... ERR=0
Battery: ... V
LoRa TX: TYPE=DATA,...
ACK received: TYPE=ACK,...
Sleep duration: ... min
```

Log gateway đúng:

```text
===== EE4552 GATEWAY FIRMWARE =====
Firmware: GW_OTA_TEST_1
LoRa init OK
Gateway IP: ...
HTTP server listening on port 80
Debug HTTP server listening on port 8080
Gateway ready
DATA RX node=1 pid=...
ThingsBoard HTTP status: 200
ACK TX: TYPE=ACK,...
```

Gợi ý lỗi thường gặp:

- `LoRa init failed`: kiểm tra nguồn 3.3V, anten, dây SPI, GND chung.
- `DHT ... ERR=1`: kiểm tra DATA `GPIO27`, pull-up và nguồn DHT22.
- `SOIL ... ERR=1`: kiểm tra `AOUT -> GPIO34`, nguồn cảm biến và ADC có nằm ngoài `100..4090` không.
- Pin sai: kiểm tra chia áp `220k/100k`, điểm giữa vào `GPIO35`.
- Gateway không upload cloud: kiểm tra WiFi, ThingsBoard token và `EnableCloudUpload`.

## 16. Giới Hạn Hiện Tại

- Mặc định chỉ dùng `NodeId = 1`; gateway có mảng tracking 256 node nhưng chưa có quản lý node nâng cao.
- History gateway chỉ là RAM 64 mẫu, mất nguồn là mất history.
- ThingsBoard dùng HTTP, chưa dùng MQTT.
- `PumpTime` là gợi ý/mô phỏng trong cấu hình hiện tại.
- `SET_THRESHOLD`, `SET_FILTER_MODE`, `SET_PUMP_TIME` và `SET_CONTROL_MODE` được lưu vào NVS và gửi lên dashboard/cloud, nhưng thuật toán phân loại đất, lọc soil và tính `PumpTime` hiện vẫn dùng logic cố định trong code.
- Node OTA qua LoRa là mô phỏng protocol, không flash firmware thật.
- Gateway self-OTA qua WiFi là OTA thật.

## 17. Tóm Tắt Một Chu Kỳ

```text
Node wake
  -> bật GPIO32 cấp nguồn cảm biến
  -> đọc DHT22, soil ADC, battery ADC
  -> tạo TYPE=DATA + CRC, kèm config node
  -> gửi LoRa
  -> chờ TYPE=ACK
  -> thực thi command nếu có
  -> tính sleep fixed/adaptive
  -> tắt LoRa/SPI, kéo GPIO32 LOW
  -> deep sleep

Gateway chạy liên tục
  -> nghe LoRa
  -> nhận DATA hoặc OTA_STATUS
  -> check CRC, chống trùng PID
  -> lưu history RAM
  -> tính Alert/PumpTime
  -> upload ThingsBoard
  -> gửi ACK/command về node
  -> phục vụ dashboard, CSV, command, self-OTA qua HTTP
```
