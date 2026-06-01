# Huong dan ket noi phan cung EE4552 WSN

Tai lieu nay mo ta phan cung dung voi code hien tai trong thu muc `src/`.
He thong gom 2 mach tach rieng:

- `Gateway`: ESP32 + LoRa RA-02, nhan du lieu tu node, ket noi WiFi va day du lieu len ThingsBoard/dashboard.
- `Node`: ESP32 + LoRa RA-02 + DHT22 + cam bien do am dat dien dung + mach do dien ap pin.

Khong con mach bom trong phan cung. Trong code van con ten lenh/cau hinh `PumpTime` de mo phong tinh nang tuoi tieu, nhung `EnablePumpHardware = false`, nen he thong chi tinh thoi gian tuoi goi y va in/hien thi len Serial, dashboard, ThingsBoard.

## 1. Cau hinh chan dang dung

Gia tri lay tu `src/project_config.h`.

| Chuc nang | GPIO ESP32 | Dung tren | Ghi chu |
| --- | ---: | --- | --- |
| DHT22 DATA | GPIO27 | Node | Tin hieu 1-wire cua DHT22 |
| Soil sensor AOUT | GPIO34 | Node | ADC1, input only |
| Battery ADC | GPIO35 | Node | ADC1, input only, nhan diem giua mach chia ap |
| Sensor power control | GPIO32 | Node | Code bat HIGH khi do, LOW truoc sleep |
| LoRa NSS / CS | GPIO5 | Node va Gateway | SPI chip select |
| LoRa RST | GPIO14 | Node va Gateway | Reset module LoRa |
| LoRa DIO0 | GPIO26 | Node va Gateway | Chan interrupt RX/TX cua RA-02 |
| LoRa SCK | GPIO18 | Node va Gateway | SPI clock |
| LoRa MISO | GPIO19 | Node va Gateway | SPI MISO |
| LoRa MOSI | GPIO23 | Node va Gateway | SPI MOSI |

Thong so LoRa trong code:

| Thong so | Gia tri |
| --- | --- |
| Frequency | 433 MHz |
| Spreading factor | 7 |
| Signal bandwidth | 125 kHz |
| Coding rate | 4/5 |
| TX power | 17 dBm |

## 2. Gateway: ESP32 + LoRa RA-02

Gateway chi can ESP32, module LoRa RA-02, nguon cap on dinh va WiFi. Khong can DHT22, cam bien dat, mach chia ap pin hay mach bom.

### 2.1. Bang noi day Gateway

| LoRa RA-02 | ESP32 Gateway | Ghi chu |
| --- | --- | --- |
| `3.3V` | `3V3` | RA-02 chi dung 3.3V, khong cap 5V |
| `GND` | `GND` | Noi chung mass |
| `NSS` / `CS` | `GPIO5` | Chip select SPI |
| `SCK` | `GPIO18` | SPI clock |
| `MISO` | `GPIO19` | SPI MISO |
| `MOSI` | `GPIO23` | SPI MOSI |
| `RST` | `GPIO14` | Reset LoRa |
| `DIO0` | `GPIO26` | Interrupt LoRa |

### 2.2. Tu loc nhieu cho LoRa Gateway

RA-02 de bi reset, khoi tao loi hoac truyen nhan kem neu nguon 3.3V sut ap. Nen lap tu loc ngay sat chan cap nguon cua module:

| Linh kien | Cach noi | Muc dich |
| --- | --- | --- |
| Tu gom `104` / `100nF` | Mac song song giua `3.3V` va `GND` cua RA-02 | Loc nhieu tan so cao |
| Tu hoa `10uF` den `100uF` | Chan `+` vao `3.3V`, chan `-` vao `GND` cua RA-02 | Giam sut ap khi LoRa phat |

Luu y khi lap:

- Gan anten truoc khi cap nguon va truyen LoRa.
- Day SPI nen ngan, cam chac, tranh di qua day nguon dong lon.
- GND cua ESP32 va RA-02 phai noi chung.
- Neu Serial bao `LoRa init failed`, kiem tra lai nguon 3.3V, anten, day SPI, chan `RST`, `NSS`, `DIO0`, va them tu hoa gan RA-02.

### 2.3. Checklist Gateway

- RA-02 cap `3.3V`, khong cap `5V`.
- Da gan anten LoRa.
- `SCK=18`, `MISO=19`, `MOSI=23`, `NSS=5`, `RST=14`, `DIO0=26`.
- Co tu gom `100nF` va nen co tu hoa `10uF..100uF` sat module LoRa.
- ESP32 Gateway co nguon USB/adapter on dinh de chay WiFi lien tuc.

## 3. Node: ESP32 + LoRa + cam bien

Node la mach ngoai dong, doc cam bien, do pin, gui packet LoRa ve Gateway, cho ACK/command, sau do vao deep sleep.

Linh kien tren Node:

- 1 ESP32 DevKit.
- 1 LoRa RA-02.
- 1 DHT22.
- 1 cam bien do am dat dien dung co ngo ra analog `AOUT`.
- 1 dien tro `220k ohm` va 1 dien tro `100k ohm` de chia ap do pin.
- Tu gom `100nF` va tu hoa `10uF..100uF` de loc nguon LoRa/cam bien.

### 3.1. LoRa RA-02 tren Node

Noi giong Gateway:

| LoRa RA-02 | ESP32 Node | Ghi chu |
| --- | --- | --- |
| `3.3V` | `3V3` | Khong cap 5V vao RA-02 |
| `GND` | `GND` | Noi chung mass |
| `NSS` / `CS` | `GPIO5` | SPI chip select |
| `SCK` | `GPIO18` | SPI clock |
| `MISO` | `GPIO19` | SPI MISO |
| `MOSI` | `GPIO23` | SPI MOSI |
| `RST` | `GPIO14` | Reset LoRa |
| `DIO0` | `GPIO26` | Interrupt LoRa |

Tu loc nhieu cho LoRa Node:

- Tu gom `100nF`: mac song song `3.3V-GND` cua RA-02, dat cang gan chan module cang tot.
- Tu hoa `10uF..100uF`: mac song song `3.3V-GND` cua RA-02, chan `+` vao `3.3V`, chan `-` vao `GND`.
- Neu dung pin hoac breadboard, nen dung day nguon ngan va dau noi chac vi RA-02 hut dong cao khi phat.

### 3.2. DHT22 tren Node

| DHT22 | ESP32 Node | Ghi chu |
| --- | --- | --- |
| `VCC` / `+` | `3V3` | Co the cap 3.3V |
| `GND` / `-` | `GND` | Noi chung mass |
| `DATA` / `OUT` | `GPIO27` | Chan DHT trong code |

Luu y:

- Neu dung module DHT22 3 chan, thuong module da co dien tro keo len.
- Neu dung DHT22 raw 4 chan, them dien tro keo len `4.7k..10k` giua `DATA` va `3V3`.
- Nen dat tu gom `100nF` giua `VCC-GND` cua DHT22 neu day dai hoac nguon nhieu.

### 3.3. Cam bien do am dat dien dung

| Cam bien do am dat | ESP32 Node | Ghi chu |
| --- | --- | --- |
| `VCC` | `3V3` | Cap nguon cam bien |
| `GND` | `GND` | Noi chung mass |
| `AOUT` | `GPIO34` | Ngo vao ADC1 |

Trong code, cam bien duoc hieu chuan theo:

| Moc hieu chuan | Gia tri ADC |
| --- | ---: |
| Dat kho / dry | `3400` |
| Dat uot / wet | `1200` |
| ADC hop le nho nhat | `100` |
| ADC hop le lon nhat | `4090` |

Cong thuc tinh do am dat:

```text
H_soil = (ADC_dry - ADC_filtered) * 60 / (ADC_dry - ADC_wet)
```

Ket qua duoc lam tron 0.1 va gioi han trong `0..60 %Vol`.

Phan loai do am dat:

| H_soil | Trang thai |
| ---: | --- |
| `> 42 %Vol` | `OVER_MOISTURE` |
| `> 33..42 %Vol` | `NORMAL` |
| `> 24..33 %Vol` | `LIGHT_DRY` |
| `> 15..24 %Vol` | `NEED_WATERING` |
| `<= 15 %Vol` | `URGENT_WATERING` |

### 3.4. Mach chia ap do pin bang 220k va 100k

ESP32 khong duoc do truc tiep dien ap pin/nguon cao hon muc ADC. Phai dung mach chia ap dua dien ap ve GPIO35.

Code dang cau hinh:

```text
BatteryAdcPin = GPIO35
BatteryDividerRatio = 3.2
Rtop = 220k
Rbottom = 100k
```

Anh minh hoa mach chia ap:

![Mach chia ap do pin](Voltage_divider_circuit.png)

So do noi day:

```text
VIN / Vbat / nguon can do
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
| Dau tren dien tro `220k` | Cuc duong pin / `VIN` / nguon can do |
| Dau duoi dien tro `220k` | Diem giua mach chia ap |
| Dau tren dien tro `100k` | Diem giua mach chia ap |
| Dau duoi dien tro `100k` | `GND` |
| Diem giua `220k-100k` | `GPIO35` |
| GND cua pin/nguon | `GND` ESP32 |

Vi du neu nguon can do la `5.0V`:

```text
V_gpio35 = 5.0 * 100k / (220k + 100k) = 1.56V
V_bat = V_gpio35 * 3.2
```

Luu y:

- Khong noi pin/5V truc tiep vao `GPIO35`.
- `GPIO35` chi la input, khong xuat tin hieu.
- GND cua pin/adapter phai noi chung voi GND ESP32.
- Neu dong ho do thuc te sai voi gia tri Serial, hieu chinh `BatteryDividerRatio` trong `project_config.h`.

### 3.5. Sensor power GPIO32

Code cau hinh `SensorPowerPin = GPIO32`. Hien tai trong `node_main.cpp`, GPIO32 duoc keo HIGH khi node thuc day va keo LOW truoc khi deep sleep.

Neu ban chi lap mach don gian:

- Co the cap truc tiep DHT22 va cam bien dat tu `3V3`.
- GPIO32 luc nay chi la chan dieu khien du phong theo code.

Neu muon tiet kiem pin tot hon:

- Dung GPIO32 de dieu khien MOSFET/load switch cap nguon cho cam bien.
- Khong cap truc tiep dong lon tu GPIO32 cho cam bien neu chua tinh dong tai.

## 4. Bang noi day Node day du

| Thiet bi | Chan thiet bi | Noi den ESP32 Node |
| --- | --- | --- |
| LoRa RA-02 | `3.3V` | `3V3` |
| LoRa RA-02 | `GND` | `GND` |
| LoRa RA-02 | `NSS` / `CS` | `GPIO5` |
| LoRa RA-02 | `SCK` | `GPIO18` |
| LoRa RA-02 | `MISO` | `GPIO19` |
| LoRa RA-02 | `MOSI` | `GPIO23` |
| LoRa RA-02 | `RST` | `GPIO14` |
| LoRa RA-02 | `DIO0` | `GPIO26` |
| DHT22 | `VCC` | `3V3` |
| DHT22 | `GND` | `GND` |
| DHT22 | `DATA` / `OUT` | `GPIO27` |
| Soil sensor | `VCC` | `3V3` |
| Soil sensor | `GND` | `GND` |
| Soil sensor | `AOUT` | `GPIO34` |
| Battery divider | Dau tren `220k` | Cuc duong pin / nguon can do |
| Battery divider | Diem giua `220k-100k` | `GPIO35` |
| Battery divider | Dau duoi `100k` | `GND` |
| Tu gom LoRa | `100nF` | Song song `3.3V-GND` RA-02 |
| Tu hoa LoRa | `10uF..100uF` | Song song `3.3V-GND` RA-02 |
| Tu gom cam bien | `100nF` | Song song `VCC-GND` DHT22/cam bien dat neu can |

## 5. Loi thuong gap

| Hien tuong | Nguyen nhan hay gap | Cach kiem tra |
| --- | --- | --- |
| `LoRa init failed` | Sai day SPI/RST/NSS, thieu anten, nguon 3.3V yeu | Do lai 3.3V, them tu hoa, kiem tra GPIO theo bang |
| Gateway khong nhan du lieu | Sai tan so/module, node khong gui, anten kem | Ca hai phai dung RA-02 433 MHz va code cung `433E6` |
| DHT bao loi | Thieu pull-up, sai chan DATA, day dai | DATA vao GPIO27, them pull-up `4.7k..10k` neu cam bien raw |
| Do am dat luon 0 hoac 60 | Sai AOUT/GPIO, cam bien chua calib | AOUT vao GPIO34, xem `ADC` tren Serial |
| Dien ap pin sai | Sai vi tri 220k/100k, chua noi chung GND | Diem giua vao GPIO35, GND pin noi GND ESP32 |

## 6. Dau hieu test dung

Node Serial:

```text
===== EE4552 NODE FIRMWARE =====
LoRa init OK
DHT: T=... C H=... %RH ERR=0
SOIL: ADC=... H=... %Vol status=... ERR=0
Battery: ... V
LoRa TX try 1: TYPE=DATA,...
ACK received: TYPE=ACK,...
Sleep duration: ... min
```

Gateway Serial:

```text
===== EE4552 GATEWAY FIRMWARE =====
LoRa init OK
Gateway ready
DATA RX node=1 pid=... T=... H=... Soil=... ADC=... V=... RSSI=...
ThingsBoard HTTP status: 200
ACK TX: TYPE=ACK,...
```

Gateway web dashboard:

```text
http://<IP-cua-Gateway>/
```

Xuat CSV:

```text
http://<IP-cua-Gateway>/export.csv
```
