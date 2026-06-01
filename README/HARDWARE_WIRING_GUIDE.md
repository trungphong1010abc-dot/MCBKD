# Hướng dẫn kết nối phần cứng EE4552 WSN

File này tổng hợp lại cách nối dây theo code hiện tại trong `src/project_config.h`.
Dùng file này để lắp lại mạch nhanh sau khi tháo breadboard.

## 1. Tổng quan phần cứng

Hệ thống có 2 ESP32:

- `Node`: đọc DHT22, cảm biến độ ẩm đất, đo điện áp pin, gửi LoRa.
- `Gateway`: nhận LoRa từ node, kết nối WiFi và gửi dữ liệu lên ThingsBoard.

Cả `Node` và `Gateway` đều dùng cùng sơ đồ chân LoRa RA-02.
Chỉ `Node` mới cần DHT22, cảm biến đất và mạch chia áp pin.

## 2. Bảng chân ESP32 đang dùng trong code

| Chức năng | ESP32 GPIO | Ghi chú |
| --- | ---: | --- |
| DHT22 DATA | GPIO27 | Node |
| Soil sensor AOUT | GPIO34 | Node, ADC1, input only |
| Battery ADC | GPIO35 | Node, ADC1, input only |
| Sensor power control | GPIO32 | Node, dự phòng điều khiển nguồn cảm biến |
| Pump control placeholder | GPIO25 | Node, hiện đang không bật phần cứng bơm |
| LoRa NSS / CS | GPIO5 | Node và Gateway |
| LoRa RST | GPIO14 | Node và Gateway |
| LoRa DIO0 | GPIO26 | Node và Gateway |
| LoRa SCK | GPIO18 | Node và Gateway |
| LoRa MISO | GPIO19 | Node và Gateway |
| LoRa MOSI | GPIO23 | Node và Gateway |

## 3. Kết nối LoRa RA-02 với ESP32

Dùng bảng này cho cả `Node` và `Gateway`.

| LoRa RA-02 | ESP32 | Ghi chú |
| --- | --- | --- |
| `3.3V` | `3V3` | Không cấp 5V vào RA-02 |
| `GND` | `GND` | Nối chung mass |
| `NSS` / `CS` | `GPIO5` | Chip select SPI |
| `SCK` | `GPIO18` | SPI clock |
| `MISO` | `GPIO19` | SPI MISO |
| `MOSI` | `GPIO23` | SPI MOSI |
| `RST` | `GPIO14` | Reset LoRa |
| `DIO0` | `GPIO26` | Interrupt RX/TX |

Khuyến nghị phần cứng cho LoRa:

- Gắn anten trước khi truyền.
- Đặt 1 tụ gốm `104` / `100nF` sát chân `3.3V-GND` của RA-02.
- Nên thêm 1 tụ hóa `10uF` đến `100uF` sát chân `3.3V-GND` của RA-02 nếu hay gặp `LoRa init failed`.
- Dây SPI nên ngắn, cắm chắc, GND phải nối chung với ESP32.

## 4. Kết nối DHT22 với Node ESP32

| DHT22 | ESP32 Node | Ghi chú |
| --- | --- | --- |
| `VCC` / `+` | `3V3` | Có thể dùng 3.3V |
| `GND` / `-` | `GND` | Nối chung mass |
| `DATA` / `OUT` | `GPIO27` | Chân đọc DHT22 trong code |

Ghi chú:

- Nếu DHT22 đang là cảm biến rời 3 chân module thì thường đã có điện trở kéo lên.
- Nếu dùng DHT22 raw 4 chân, nên thêm điện trở kéo lên `4.7k` đến `10k` giữa `DATA` và `3V3`.
- Đặt 1 tụ gốm `104` / `100nF` gần `VCC-GND` của DHT22 nếu dây dài.

## 5. Kết nối cảm biến độ ẩm đất điện dung với Node ESP32

| Cảm biến độ ẩm đất | ESP32 Node | Ghi chú |
| --- | --- | --- |
| `VCC` | `3V3` | Cấp nguồn cảm biến |
| `GND` | `GND` | Nối chung mass |
| `AOUT` | `GPIO34` | ADC1 input |

Ghi chú calib đang dùng trong code:

| Mốc calib | Giá trị ADC |
| --- | ---: |
| Đất khô / dry | `3400` |
| Đất ướt / wet | `1200` |

Công thức đang dùng:

```text
H_soil = (ADC_dry - ADC_filtered) * 60 / (ADC_dry - ADC_wet)
```

Kết quả bị giới hạn trong khoảng `0..60 %Vol`.

## 6. Mạch chia áp đo pin bằng 220k và 100k

ESP32 ADC không được đo trực tiếp điện áp pin/nguồn 5V. Phải dùng mạch chia áp.
Code đang dùng:

```text
BatteryAdcPin = GPIO35
BatteryDividerRatio = 3.2
Rtop = 220k
Rbottom = 100k
```

Sơ đồ nối:

```text
VIN / Vbat / 5V cần đo
        |
      220k
        |
        +----> GPIO35 của ESP32
        |
      100k
        |
       GND
```

Bảng nối chi tiết:

| Điểm mạch | Nối tới đâu |
| --- | --- |
| Đầu trên điện trở `220k` | `VIN` / cực dương pin / nguồn cần đo |
| Đầu dưới điện trở `220k` | Nối chung với đầu trên `100k` và `GPIO35` |
| Đầu trên điện trở `100k` | Điểm giữa mạch chia áp |
| Đầu dưới điện trở `100k` | `GND` |
| Điểm giữa `220k-100k` | `GPIO35` |
| GND nguồn/pin | `GND` ESP32 |

Ví dụ nếu nguồn cần đo là `5.0V`, điện áp tại `GPIO35` xấp xỉ:

```text
V_gpio35 = 5.0 * 100k / (220k + 100k) = 1.56V
```

Code sẽ nhân ngược với hệ số `3.2`:

```text
V_bat = V_gpio35 * 3.2
```

Lưu ý:

- Không nối `VIN/5V` trực tiếp vào `GPIO35`.
- GPIO35 chỉ là input, không xuất được tín hiệu.
- Nối chung GND của pin/adapter với GND ESP32.
- Nếu số đo sai lệch vài phần trăm, có thể hiệu chỉnh `BatteryDividerRatio` theo đồng hồ đo thực tế.

## 7. Kết nối Node đầy đủ

| Thiết bị | Chân thiết bị | Nối đến ESP32 Node |
| --- | --- | --- |
| LoRa RA-02 | `3.3V` | `3V3` |
| LoRa RA-02 | `GND` | `GND` |
| LoRa RA-02 | `NSS` | `GPIO5` |
| LoRa RA-02 | `SCK` | `GPIO18` |
| LoRa RA-02 | `MISO` | `GPIO19` |
| LoRa RA-02 | `MOSI` | `GPIO23` |
| LoRa RA-02 | `RST` | `GPIO14` |
| LoRa RA-02 | `DIO0` | `GPIO26` |
| DHT22 | `VCC` | `3V3` |
| DHT22 | `GND` | `GND` |
| DHT22 | `DATA` | `GPIO27` |
| Soil sensor | `VCC` | `3V3` |
| Soil sensor | `GND` | `GND` |
| Soil sensor | `AOUT` | `GPIO34` |
| Battery divider | Điểm giữa `220k-100k` | `GPIO35` |
| Battery divider | Đầu trên `220k` | `VIN` / nguồn cần đo |
| Battery divider | Đầu dưới `100k` | `GND` |

## 8. Kết nối Gateway đầy đủ

Gateway chỉ cần ESP32 + LoRa RA-02 + WiFi.

| Thiết bị | Chân thiết bị | Nối đến ESP32 Gateway |
| --- | --- | --- |
| LoRa RA-02 | `3.3V` | `3V3` |
| LoRa RA-02 | `GND` | `GND` |
| LoRa RA-02 | `NSS` | `GPIO5` |
| LoRa RA-02 | `SCK` | `GPIO18` |
| LoRa RA-02 | `MISO` | `GPIO19` |
| LoRa RA-02 | `MOSI` | `GPIO23` |
| LoRa RA-02 | `RST` | `GPIO14` |
| LoRa RA-02 | `DIO0` | `GPIO26` |

## 9. Checklist trước khi cấp nguồn

- LoRa RA-02 chỉ cấp `3.3V`, không cấp `5V`.
- Node và gateway đều gắn anten LoRa.
- Tất cả GND nối chung trong từng mạch.
- Soil sensor `AOUT` vào `GPIO34`, không vào GPIO35.
- Mạch chia áp pin vào `GPIO35`, không vào GPIO34.
- DHT22 DATA vào `GPIO27`.
- LoRa SPI đúng: `SCK=18`, `MISO=19`, `MOSI=23`, `NSS=5`, `RST=14`, `DIO0=26`.
- Tụ gốm `104` gần LoRa và cảm biến; nên có thêm tụ hóa gần LoRa nếu breadboard không ổn định.

## 10. Dấu hiệu test đúng

Node serial:

```text
LoRa init OK
DHT: T=... H=... ERR=0
SOIL: ADC=... H=... %Vol status=...
Battery: ... V
LoRa TX try 1: ...
ACK received: ...
Sleep duration: 5 min
```

Gateway serial:

```text
LoRa init OK
Gateway ready
DATA RX node=1 pid=...
ThingsBoard HTTP status: 200
ACK TX: ...
```
