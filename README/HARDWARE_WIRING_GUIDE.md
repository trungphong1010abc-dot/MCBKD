# Huong dan ket noi phan cung EE4552 WSN

File nay tong hop lai cach noi day theo code hien tai trong `src/project_config.h`.
Dung file nay de lap lai mach nhanh sau khi thao breadboard.

## 1. Tong quan phan cung

He thong co 2 ESP32:

- `Node`: doc DHT22, cam bien do am dat, do dien ap pin, gui LoRa.
- `Gateway`: nhan LoRa tu node, ket noi WiFi va gui du lieu len ThingsBoard.

Ca `Node` va `Gateway` deu dung cung so do chan LoRa RA-02.
Chi `Node` moi can DHT22, cam bien dat va mach chia ap pin.

## 2. Bang chan ESP32 dang dung trong code

| Chuc nang | ESP32 GPIO | Ghi chu |
| --- | ---: | --- |
| DHT22 DATA | GPIO27 | Node |
| Soil sensor AOUT | GPIO34 | Node, ADC1, input only |
| Battery ADC | GPIO35 | Node, ADC1, input only |
| Sensor power control | GPIO32 | Node, du phong dieu khien nguon cam bien |
| Pump control placeholder | GPIO25 | Node, hien dang khong bat phan cung bom |
| LoRa NSS / CS | GPIO5 | Node va Gateway |
| LoRa RST | GPIO14 | Node va Gateway |
| LoRa DIO0 | GPIO26 | Node va Gateway |
| LoRa SCK | GPIO18 | Node va Gateway |
| LoRa MISO | GPIO19 | Node va Gateway |
| LoRa MOSI | GPIO23 | Node va Gateway |

## 3. Ket noi LoRa RA-02 voi ESP32

Dung bang nay cho ca `Node` va `Gateway`.

| LoRa RA-02 | ESP32 | Ghi chu |
| --- | --- | --- |
| `3.3V` | `3V3` | Khong cap 5V vao RA-02 |
| `GND` | `GND` | Noi chung mass |
| `NSS` / `CS` | `GPIO5` | Chip select SPI |
| `SCK` | `GPIO18` | SPI clock |
| `MISO` | `GPIO19` | SPI MISO |
| `MOSI` | `GPIO23` | SPI MOSI |
| `RST` | `GPIO14` | Reset LoRa |
| `DIO0` | `GPIO26` | Interrupt RX/TX |

Khuyen nghi phan cung cho LoRa:

- Gan anten truoc khi truyen.
- Dat 1 tu gom `104` / `100nF` sat chan `3.3V-GND` cua RA-02.
- Nen them 1 tu hoa `10uF` den `100uF` sat chan `3.3V-GND` cua RA-02 neu hay gap `LoRa init failed`.
- Day SPI nen ngan, cam chac, GND phai noi chung voi ESP32.

## 4. Ket noi DHT22 voi Node ESP32

| DHT22 | ESP32 Node | Ghi chu |
| --- | --- | --- |
| `VCC` / `+` | `3V3` | Co the dung 3.3V |
| `GND` / `-` | `GND` | Noi chung mass |
| `DATA` / `OUT` | `GPIO27` | Chan doc DHT22 trong code |

Ghi chu:

- Neu DHT22 dang la cam bien roi 3 chan module thi thuong da co dien tro keo len.
- Neu dung DHT22 raw 4 chan, nen them dien tro keo len `4.7k` den `10k` giua `DATA` va `3V3`.
- Dat 1 tu gom `104` / `100nF` gan `VCC-GND` cua DHT22 neu day dai.

## 5. Ket noi cam bien do am dat dien dung voi Node ESP32

| Cam bien do am dat | ESP32 Node | Ghi chu |
| --- | --- | --- |
| `VCC` | `3V3` | Cap nguon cam bien |
| `GND` | `GND` | Noi chung mass |
| `AOUT` | `GPIO34` | ADC1 input |

Ghi chu calib dang dung trong code:

| Moc calib | Gia tri ADC |
| --- | ---: |
| Dat kho / dry | `3400` |
| Dat uot / wet | `1200` |

Cong thuc dang dung:

```text
H_soil = (ADC_dry - ADC_filtered) * 60 / (ADC_dry - ADC_wet)
```

Ket qua bi gioi han trong khoang `0..60 %Vol`.

## 6. Mach chia ap do pin bang 220k va 100k

ESP32 ADC khong duoc do truc tiep dien ap pin/nguon 5V. Phai dung mach chia ap.
Code dang dung:

```text
BatteryAdcPin = GPIO35
BatteryDividerRatio = 3.2
Rtop = 220k
Rbottom = 100k
```

So do noi:

```text
VIN / Vbat / 5V can do
        |
      220k
        |
        +----> GPIO35 cua ESP32
        |
      100k
        |
       GND
```

Bang noi chi tiet:

| Diem mach | Noi toi dau |
| --- | --- |
| Dau tren dien tro `220k` | `VIN` / cuc duong pin / nguon can do |
| Dau duoi dien tro `220k` | Noi chung voi dau tren `100k` va `GPIO35` |
| Dau tren dien tro `100k` | Diem giua mach chia ap |
| Dau duoi dien tro `100k` | `GND` |
| Diem giua `220k-100k` | `GPIO35` |
| GND nguon/pin | `GND` ESP32 |

Vi du neu nguon can do la `5.0V`, dien ap tai `GPIO35` xap xi:

```text
V_gpio35 = 5.0 * 100k / (220k + 100k) = 1.56V
```

Code se nhan nguoc voi he so `3.2`:

```text
V_bat = V_gpio35 * 3.2
```

Luu y:

- Khong noi `VIN/5V` truc tiep vao `GPIO35`.
- GPIO35 chi la input, khong xuat duoc tin hieu.
- Noi chung GND cua pin/adapter voi GND ESP32.
- Neu so do sai lech vai phan tram, co the hieu chinh `BatteryDividerRatio` theo dong ho do thuc te.

## 7. Ket noi Node day du

| Thiet bi | Chan thiet bi | Noi den ESP32 Node |
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
| Battery divider | Diem giua `220k-100k` | `GPIO35` |
| Battery divider | Dau tren `220k` | `VIN` / nguon can do |
| Battery divider | Dau duoi `100k` | `GND` |

## 8. Ket noi Gateway day du

Gateway chi can ESP32 + LoRa RA-02 + WiFi.

| Thiet bi | Chan thiet bi | Noi den ESP32 Gateway |
| --- | --- | --- |
| LoRa RA-02 | `3.3V` | `3V3` |
| LoRa RA-02 | `GND` | `GND` |
| LoRa RA-02 | `NSS` | `GPIO5` |
| LoRa RA-02 | `SCK` | `GPIO18` |
| LoRa RA-02 | `MISO` | `GPIO19` |
| LoRa RA-02 | `MOSI` | `GPIO23` |
| LoRa RA-02 | `RST` | `GPIO14` |
| LoRa RA-02 | `DIO0` | `GPIO26` |

## 9. Checklist truoc khi cap nguon

- LoRa RA-02 chi cap `3.3V`, khong cap `5V`.
- Node va gateway deu gan anten LoRa.
- Tat ca GND noi chung trong tung mach.
- Soil sensor `AOUT` vao `GPIO34`, khong vao GPIO35.
- Mach chia ap pin vao `GPIO35`, khong vao GPIO34.
- DHT22 DATA vao `GPIO27`.
- LoRa SPI dung: `SCK=18`, `MISO=19`, `MOSI=23`, `NSS=5`, `RST=14`, `DIO0=26`.
- Tu gom `104` gan LoRa va cam bien; nen co them tu hoa gan LoRa neu breadboard khong on dinh.

## 10. Dau hieu test dung

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

