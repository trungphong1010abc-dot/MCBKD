# Hướng Dẫn Kết Nối Phần Cứng EE4552 WSN

Tài liệu này được cập nhật theo code hiện tại trong `src/project_config.h`, `src/node_main.cpp` và `src/gateway_*.cpp`.

Hệ thống có 2 ESP32:

- `node`: đọc DHT22, cảm biến độ ẩm đất, điện áp pin, gửi telemetry qua LoRa rồi deep sleep.
- `gateway`: nhận LoRa, kết nối WiFi, mở dashboard HTTP, xuất CSV, gửi telemetry lên ThingsBoard và trả ACK/command cho node.

## 1. Bảng Chân Đang Dùng

| Chức năng | GPIO ESP32 | Dùng trên | Ghi chú |
| --- | ---: | --- | --- |
| DHT22 DATA | 27 | Node | Driver DHT22 tự viết |
| Soil sensor AOUT | 34 | Node | ADC1, input-only |
| Battery ADC | 35 | Node | ADC1, input-only, qua chia áp 220k/100k |
| Sensor power control | 32 | Node | HIGH khi node thức, LOW trước deep sleep |
| Pump output legacy/test | 25 | Node | `EnablePumpHardware=false`; không khuyến nghị đấu bơm thật trong cấu hình hiện tại |
| LoRa NSS / CS | 5 | Node + Gateway | SPI chip select |
| LoRa RST | 14 | Node + Gateway | Reset module LoRa |
| LoRa DIO0 | 26 | Node + Gateway | Interrupt RX/TX |
| LoRa SCK | 18 | Node + Gateway | SPI clock |
| LoRa MISO | 19 | Node + Gateway | SPI MISO |
| LoRa MOSI | 23 | Node + Gateway | SPI MOSI |

## 2. Kết Nối LoRa RA-02 Với ESP32

Dùng cùng một sơ đồ cho cả node và gateway.

| LoRa RA-02 | ESP32 | Ghi chú |
| --- | --- | --- |
| `3.3V` | `3V3` | Không cấp 5V vào RA-02 |
| `GND` | `GND` | GND phải nối chung |
| `NSS` / `CS` | `GPIO5` | SPI chip select |
| `SCK` | `GPIO18` | SPI clock |
| `MISO` | `GPIO19` | SPI MISO |
| `MOSI` | `GPIO23` | SPI MOSI |
| `RST` | `GPIO14` | Reset LoRa |
| `DIO0` | `GPIO26` | Interrupt |

Khuyến nghị:

- Luôn gắn anten trước khi truyền LoRa.
- RA-02 chỉ dùng 3.3V; cấp 5V có thể làm hỏng module.
- Đặt tụ gốm `100nF` sát chân `3.3V-GND` của RA-02.
- Nếu hay gặp `LoRa init failed`, thêm tụ `10uF..100uF` gần module LoRa và rút ngắn dây SPI.

## 3. Kết Nối DHT22 Với Node

| DHT22 | ESP32 Node | Ghi chú |
| --- | --- | --- |
| `VCC` / `+` | `GPIO32` hoặc `3V3` | Khuyến nghị `GPIO32` nếu muốn tắt nguồn cảm biến khi deep sleep |
| `GND` / `-` | `GND` | GND chung |
| `DATA` / `OUT` | `GPIO27` | Pin đọc trong code |

Ghi chú:

- `GPIO32` được code kéo HIGH khi node thức và LOW trước khi ngủ. Nếu module DHT22 tiêu thụ thấp, có thể cấp qua chân này để tiết kiệm pin.
- Nếu muốn cấp nguồn ổn định liên tục, nối `VCC` DHT22 vào `3V3`.
- Module DHT22 3 chân thường đã có điện trở kéo lên. Nếu dùng cảm biến raw 4 chân, thêm điện trở `4.7k..10k` giữa `DATA` và `VCC`.

## 4. Kết Nối Cảm Biến Độ Ẩm Đất Điện Dung

| Cảm biến đất | ESP32 Node | Ghi chú |
| --- | --- | --- |
| `VCC` | `GPIO32` hoặc `3V3` | Khuyến nghị `GPIO32` để tắt nguồn khi deep sleep |
| `GND` | `GND` | GND chung |
| `AOUT` | `GPIO34` | ADC1 input-only |

Code đang dùng 11 mẫu ADC, bỏ 3 mẫu đầu, lọc median và kiểm tra ADC hợp lệ trong khoảng `100..4090`.

Calibration hiện tại:

| Mốc | ADC |
| --- | ---: |
| Đất khô / dry | `3400` |
| Đất ướt / wet | `1200` |

Công thức:

```text
H_soil = (ADC_dry - ADC_filtered) * 60 / (ADC_dry - ADC_wet)
```

Kết quả được giới hạn trong `0..60 %Vol`.

## 5. Mạch Chia Áp Đo Pin

ESP32 không được đo trực tiếp pin hoặc nguồn 5V bằng ADC. Code đọc `GPIO35` và nhân với `BatteryDividerRatio = 3.2`, tương ứng mạch `220k/100k`.

```text
VIN / Vbat / 5V cần đo
        |
      220k
        |
        +----> GPIO35
        |
      100k
        |
       GND
```

| Điểm mạch | Nối tới |
| --- | --- |
| Đầu trên `220k` | Cực dương pin hoặc nguồn cần đo |
| Điểm giữa `220k-100k` | `GPIO35` |
| Đầu dưới `100k` | `GND` |
| GND nguồn/pin | `GND` ESP32 |

Ví dụ với nguồn 5.0V:

```text
V_gpio35 = 5.0 * 100k / (220k + 100k) = 1.56V
V_bat = V_gpio35 * 3.2
```

Lưu ý:

- Không nối `VIN/5V` trực tiếp vào `GPIO35`.
- `GPIO35` chỉ là input, không xuất tín hiệu được.
- Nếu số đo lệch so với đồng hồ, hiệu chỉnh `BatteryDividerRatio` trong `project_config.h`.

## 6. Kết Nối Node Đầy Đủ

| Thiết bị | Chân thiết bị | Nối đến ESP32 Node |
| --- | --- | --- |
| LoRa RA-02 | `3.3V` | `3V3` |
| LoRa RA-02 | `GND` | `GND` |
| LoRa RA-02 | `NSS` / `CS` | `GPIO5` |
| LoRa RA-02 | `SCK` | `GPIO18` |
| LoRa RA-02 | `MISO` | `GPIO19` |
| LoRa RA-02 | `MOSI` | `GPIO23` |
| LoRa RA-02 | `RST` | `GPIO14` |
| LoRa RA-02 | `DIO0` | `GPIO26` |
| DHT22 | `VCC` | `GPIO32` hoặc `3V3` |
| DHT22 | `GND` | `GND` |
| DHT22 | `DATA` | `GPIO27` |
| Soil sensor | `VCC` | `GPIO32` hoặc `3V3` |
| Soil sensor | `GND` | `GND` |
| Soil sensor | `AOUT` | `GPIO34` |
| Battery divider | Điểm giữa `220k-100k` | `GPIO35` |
| Battery divider | Đầu trên `220k` | Pin/nguồn cần đo |
| Battery divider | Đầu dưới `100k` | `GND` |

`GPIO25` là output bơm legacy/test. Gateway không tự gửi lệnh bơm vì `EnablePumpHardware=false`, nhưng nếu người dùng queue thủ công `START_PUMP`, node vẫn có thể kéo `GPIO25` trong thời gian command yêu cầu. Không đấu bơm thật trực tiếp vào GPIO; nếu cần bơm thật phải có driver transistor/MOSFET, diode bảo vệ và nguồn riêng.

## 7. Kết Nối Gateway Đầy Đủ

Gateway chỉ cần ESP32, RA-02 và WiFi.

| Thiết bị | Chân thiết bị | Nối đến ESP32 Gateway |
| --- | --- | --- |
| LoRa RA-02 | `3.3V` | `3V3` |
| LoRa RA-02 | `GND` | `GND` |
| LoRa RA-02 | `NSS` / `CS` | `GPIO5` |
| LoRa RA-02 | `SCK` | `GPIO18` |
| LoRa RA-02 | `MISO` | `GPIO19` |
| LoRa RA-02 | `MOSI` | `GPIO23` |
| LoRa RA-02 | `RST` | `GPIO14` |
| LoRa RA-02 | `DIO0` | `GPIO26` |

## 8. Checklist Trước Khi Cấp Nguồn

- RA-02 chỉ cấp `3.3V`, không cấp `5V`.
- Node và gateway đều đã gắn anten LoRa.
- GND của ESP32, LoRa, cảm biến và nguồn đo pin phải nối chung.
- Soil `AOUT` vào `GPIO34`.
- Battery divider vào `GPIO35`.
- DHT22 `DATA` vào `GPIO27`.
- LoRa SPI đúng: `SCK=18`, `MISO=19`, `MOSI=23`, `NSS=5`, `RST=14`, `DIO0=26`.
- Nếu cấp cảm biến qua `GPIO32`, bảo đảm tổng dòng cảm biến nhỏ và dây cấp nguồn ngắn.

## 9. Log Test Đúng

Node:

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

Gateway:

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
