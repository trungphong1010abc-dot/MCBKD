# Project Context - EE4552 Wireless Sensor Network

Tai lieu nay giai thich y tuong phan mem va thuat toan theo code hien tai trong thu muc `src/`.
Doc cung voi `README/HARDWARE_WIRING_GUIDE.md` la co the nam duoc ca phan cung, phan mem va luong hoat dong cua he thong.

## 1. Muc tieu du an

He thong la mang cam bien khong day de giam sat moi truong dat/khong khi tren canh dong.

Thanh phan chinh:

- `Node`: dat ngoai dong, doc cam bien DHT22, cam bien do am dat dien dung, do dien ap pin, gui du lieu bang LoRa, sau do deep sleep de tiet kiem pin.
- `Gateway`: dat noi co WiFi, nhan LoRa tu node, luu lich su tam thoi, hien thi dashboard web, xuat CSV, gui telemetry len ThingsBoard va gui ACK/command nguoc ve node.

Hien tai khong dieu khien bom that. Code co tinh `PumpTime` nhu mot gia tri mo phong/goi y tuoi tieu dua tren do am dat, nhiet do va do am khong khi. Vi `Config::EnablePumpHardware = false`, gateway khong tu dong gui lenh bat bom theo `PumpTime`. Mot so ten cu trong code van co chu `Pump`, nhung trong thiet ke phan cung hien tai khong dau noi bom.

## 2. Cau truc code

| File | Vai tro |
| --- | --- |
| `src/project_config.h` | Toan bo cau hinh chan GPIO, LoRa, WiFi, ThingsBoard, sleep, calibration |
| `src/node_main.cpp` | Firmware cho node: doc cam bien, do pin, gui LoRa, nhan ACK/command, deep sleep |
| `src/gateway_main.cpp` | Firmware cho gateway: nhan LoRa, dashboard web, CSV, ThingsBoard, command, OTA mo phong |
| `src/dht22_sensor.cpp/.h` | Driver DHT22 tu viet bang timing 1-wire |
| `src/soil_moisture.cpp/.h` | Doc ADC cam bien dat, median filter, tinh `%Vol`, phan loai trang thai |
| `src/packet_protocol.cpp/.h` | Dinh dang packet LoRa, CRC16, encode/decode telemetry, ACK, OTA chunk/status |
| `platformio.ini` | Cau hinh build rieng `env:node` va `env:gateway` |

Build:

```text
pio run -e node
pio run -e gateway
```

## 3. Cau hinh chinh

Trong `project_config.h`:

| Nhom | Gia tri quan trong |
| --- | --- |
| Node/Gateway ID | `NodeId = 1`, `GatewayId = 1` |
| DHT22 | `DhtPin = GPIO27` |
| Soil ADC | `SoilAdcPin = GPIO34` |
| Battery ADC | `BatteryAdcPin = GPIO35`, `BatteryDividerRatio = 3.2` |
| Sensor power | `SensorPowerPin = GPIO32` |
| Pump hardware | `EnablePumpHardware = false`, khong dau noi bom that |
| LoRa | `433E6`, SF7, BW125kHz, CR4/5, 17dBm |
| ACK/retry | `AckTimeoutMs = 2500`, `MaxRetry = 3` |
| Sleep | Mac dinh `30` phut, adaptive `5..90` phut |
| Gateway cloud | WiFi + ThingsBoard HTTP telemetry |

## 4. Luong hoat dong cua Node

Firmware node nam trong `node_main.cpp`.

Trinh tu chay trong `setup()`:

1. Mo Serial `115200`.
2. Bat `SensorPowerPin = GPIO32` len HIGH.
3. Dua chan legacy `GPIO25` ve OFF; phan cung hien tai khong dau noi bom vao chan nay.
4. Doc runtime config tu ESP32 Preferences:
   - nguong do am dat,
   - sleep duration,
   - filter mode,
   - pump seconds mo phong,
   - control mode,
   - duty cycle mode.
5. Khoi tao DHT22 o `GPIO27`.
6. Khoi tao cam bien dat o `GPIO34` voi calibration dry/wet.
7. Khoi tao LoRa. Node retry khoi tao toi 3 lan.
8. Doc DHT22.
9. Doc do am dat.
10. Do dien ap pin qua mach chia ap o `GPIO35`.
11. Dong goi telemetry.
12. Gui telemetry qua LoRa, cho ACK tu gateway, retry toi 3 lan neu chua nhan ACK hop le.
13. Neu ACK co command thi thuc thi command.
14. Tinh thoi gian sleep theo adaptive duty cycle.
15. Tat LoRa, keo `SensorPowerPin` LOW, vao deep sleep.

Vi `loop()` gan nhu khong dung khi deep sleep bat, moi chu ky node se thuc day, chay `setup()`, gui mot lan, roi ngu tiep.

## 5. Doc DHT22

Driver DHT22 nam trong `dht22_sensor.cpp`.

Thuat toan:

1. ESP32 keo chan DATA LOW khoang `20 ms` de gui start signal.
2. Chuyen DATA ve `INPUT_PULLUP`.
3. Doi DHT22 phan hoi bang xung LOW/HIGH.
4. Doc 40 bit thanh 5 byte:
   - byte 0-1: do am,
   - byte 2-3: nhiet do,
   - byte 4: checksum.
5. Kiem tra checksum:

```text
checksum = (B0 + B1 + B2 + B3) & 0xFF
checksum phai bang B4
```

1. Tinh gia tri:

```text
H_air = rawHumidity / 10.0 + HumidityOffsetRh
T_air = rawTemperature / 10.0 + TempOffsetC
```

1. Validate:
   - `-10 <= T_air <= 50 C`
   - `0 <= H_air <= 100 %RH`
   - bien thien lien tiep khong qua `2 C` va `5 %RH`
2. Neu hop le, dua vao bo dem trung binh truot 5 mau va lam tron 0.1.
3. Neu loi timing, checksum hoac validate, tra `errorFlag = 1`.

## 6. Doc do am dat

Driver cam bien dat nam trong `soil_moisture.cpp`.

Thuat toan:

1. Cau hinh ADC 12 bit, attenuation `ADC_11db`.
2. Doc nhieu mau ADC tu `GPIO34`.
3. Loai bo 3 mau dau de cam bien on dinh.
4. Neu ADC nam ngoai khoang hop le `100..4090`, tang bo dem loi va retry sau `350 ms`.
5. Neu loi ADC qua nguong, set `errorFlag = 1`.
6. Voi mau hop le, loc median de lay `ADC_filtered`.
7. Quy doi sang do am dat:

```text
H_soil = (ADC_dry - ADC_filtered) * 60 / (ADC_dry - ADC_wet)
ADC_dry = 3400
ADC_wet = 1200
```

1. Gioi han `H_soil` trong `0..60 %Vol`, lam tron 0.1.
2. Phan loai:

| H_soil | Soil status |
| ---: | --- |
| `> 42` | `OVER_MOISTURE` |
| `> 33..42` | `NORMAL` |
| `> 24..33` | `LIGHT_DRY` |
| `> 15..24` | `NEED_WATERING` |
| `<= 15` | `URGENT_WATERING` |

## 7. Do dien ap pin

Node do pin bang `GPIO35` qua mach chia ap `220k/100k`.

Thuat toan trong `readBatteryVoltage()`:

1. Cau hinh ADC 12 bit, attenuation `ADC_11db`.
2. Doc 16 mau bang `analogReadMilliVolts()`.
3. Lay trung binh dien ap tai GPIO35.
4. Nhan voi `BatteryDividerRatio = 3.2`:

```text
V_bat = V_gpio35 * 3.2
```

Muc pin anh huong den adaptive sleep:

- `V_bat < 3.3V`: sleep `90` phut.
- `V_bat < 3.5V`: sleep `60` phut.

## 8. Packet LoRa va CRC

Protocol nam trong `packet_protocol.cpp`.
Packet la chuoi text dang `KEY=VALUE`, co CRC16-CCITT o cuoi.

### 8.1. Telemetry Node -> Gateway

Dang packet:

```text
TYPE=DATA,NODE=1,PID=1,T=30.1,HA=70.2,HS=25,VB=4.05,ADC=2480,SOIL=LIGHT_DRY,ERR=0,CRC=....
```

Truong du lieu:

| Truong | Y nghia |
| --- | --- |
| `TYPE=DATA` | Goi telemetry |
| `NODE` | ID node |
| `PID` | Packet ID, tang theo moi lan gui |
| `T` | Nhiet do khong khi, do C |
| `HA` | Do am khong khi, `%RH` |
| `HS` | Do am dat, `%Vol` |
| `VB` | Dien ap pin |
| `ADC` | Gia tri ADC dat da loc |
| `SOIL` | Trang thai dat |
| `ERR` | Co loi cam bien hay khong |
| `CRC` | CRC16 cua chuoi truoc truong CRC |

### 8.2. ACK Gateway -> Node

Dang packet:

```text
TYPE=ACK,NODE=1,PID=1,OK=1,CMD=NONE,PARAM=0,STATUS=OK,CRC=....
```

ACK vua xac nhan gateway da nhan du lieu, vua co the kem command cho node.

Command code hien co:

| Command | Y nghia |
| --- | --- |
| `SET_SLEEP_DURATION` | Doi thoi gian sleep co dinh, 5..90 phut |
| `SET_THRESHOLD` | Doi nguong do am dat trong runtime config |
| `SET_FILTER_MODE` | Chon `AVERAGE`/`MEDIAN` theo config, hien driver dat van dung median |
| `SET_PUMP_TIME` | Doi thoi gian tuoi goi y/mo phong |
| `SET_CONTROL_MODE` | Doi `MANUAL`/`AUTO` trong config |
| `SET_DUTY_CYCLE` | Chon `FIXED`/`ADAPTIVE` |
| `SLEEP_NOW` | Lenh ngu ngay, hien tai chi log |
| `START_OTA` | Bat phien OTA mo phong qua LoRa |
| `START_PUMP` | Lenh legacy trong code; khong dung cho phan cung hien tai |

## 9. Gui LoRa va retry

Node gui telemetry bang `sendTelemetryWithRetry()`:

1. Encode telemetry thanh chuoi co CRC.
2. Gui qua LoRa.
3. Chuyen LoRa ve receive mode.
4. Cho ACK trong `2500 ms`.
5. Decode ACK va kiem tra:
   - CRC dung,
   - `NODE` dung `NodeId`,
   - `PID` trung packet vua gui.
6. Neu ACK hop le va `OK=1`, xem nhu gui thanh cong.
7. Neu fail, retry toi `MaxRetry = 3`.

Gateway khi nhan packet:

1. Decode telemetry va check CRC.
2. Bo qua packet trung `PID` cua cung node, nhung van gui lai ACK de node khong retry mai.
3. Luu packet vao history vong 64 mau.
4. In log Serial.
5. Upload ThingsBoard neu WiFi/cloud dang bat.
6. Gui ACK ve node.

## 10. Gateway dashboard, CSV va ThingsBoard

Gateway dung `WebServer` port 80.

Endpoint:

| URL | Chuc nang |
| --- | --- |
| `/` | Dashboard HTML don gian hien lich su telemetry |
| `/export.csv` | Xuat history dang CSV |
| `/command?node=1&cmd=SET_SLEEP_DURATION&param=10` | Xep command cho node |
| `/self_ota?url=http://server/firmware.bin&md5=optional` | Gateway self-OTA qua WiFi |

History luu trong RAM, toi da 64 ban ghi moi nhat. Mat nguon gateway se mat history nay.

Telemetry gui len ThingsBoard bang HTTP POST:

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
  "PumpTime": 3
}
```

## 11. Thuat toan mo phong tuoi tieu

Gateway tinh `PumpTime` trong `computePumpTime()`.
Day chi la gia tri goi y/mo phong, khong co mach bom trong huong dan phan cung.

Neu `errorFlag != 0`:

```text
PumpTime = 0
```

Theo do am dat:

| Dieu kien | PumpTime co so |
| --- | ---: |
| `H_soil > 42` | 0 s |
| `33 < H_soil <= 42` | 0 s |
| `24 < H_soil <= 33` | 3 s |
| `15 < H_soil <= 24` | 6 s |
| `H_soil <= 15` | 10 s |

Hieu chinh theo moi truong:

| Dieu kien | Dieu chinh |
| --- | ---: |
| `T_air > 32 C` | `+2 s` |
| `H_air < 50 %RH` | `+3 s` |
| `H_air > 80 %RH` | `-2 s` |

Ket qua bi gioi han:

```text
0 <= PumpTime <= 15
```

Gia tri nay duoc in trong Serial log, dua len ThingsBoard va co the hien thi tren dashboard/cloud. Gateway chi tu dong gui `START_PUMP` neu `EnablePumpHardware = true`; hien tai cau hinh la `false`, nen day la phan mo phong tinh nang.

## 12. Adaptive duty cycle

Node tinh thoi gian ngu trong `computeAdaptiveSleepSeconds()`.

Neu runtime config `dutyCycleMode = FIXED`, node dung `sleepMinutes` da luu trong Preferences.

Neu `ADAPTIVE`, node dung quy tac:

| Dieu kien | Sleep |
| --- | ---: |
| `errorFlag != 0` | 30 phut |
| `V_bat < 3.3V` | 90 phut |
| `V_bat < 3.5V` | 60 phut |
| `SOIL=URGENT_WATERING` | 5 phut |
| `SOIL=NEED_WATERING` | 10 phut |
| `SOIL=LIGHT_DRY` | 20 phut |
| `SOIL=OVER_MOISTURE` | 60 phut |
| Con lai / `NORMAL` | 30 phut |

Truoc khi sleep, node:

- Cho LoRa sleep/end.
- Tat SPI.
- Keo `SensorPowerPin` LOW.
- Goi `esp_sleep_enable_timer_wakeup()`.
- Goi `esp_deep_sleep_start()`.

## 13. Command va runtime config

Gateway khong gui command rieng le ngay lap tuc. Command duoc xep vao `pendingCommands[nodeId]`.
Khi node gui telemetry lan tiep theo, gateway dua command vao ACK. Cach nay hop voi node tiet kiem pin vi node chi nghe LoRa trong thoi gian ngan sau khi gui.

Node luu mot so cau hinh bang ESP32 Preferences namespace `node_cfg`:

| Key | Y nghia |
| --- | --- |
| `soil_th` | Nguong do am dat |
| `sleep_min` | Thoi gian sleep co dinh |
| `filter` | Mode filter |
| `pump_s` | Thoi gian tuoi goi y/mo phong |
| `ctrl` | Control mode |
| `duty` | Duty cycle mode |

Vi du goi command tu trinh duyet:

```text
http://<gateway-ip>/command?node=1&cmd=SET_SLEEP_DURATION&param=10
http://<gateway-ip>/command?node=1&cmd=SET_DUTY_CYCLE&param=ADAPTIVE
http://<gateway-ip>/command?node=1&cmd=SET_THRESHOLD&param=20
```

## 14. OTA trong code

Code co 2 co che OTA:

### 14.1. Gateway self-OTA qua WiFi

Endpoint:

```text
/self_ota?url=http://server/firmware.bin&md5=optional_md5
```

Gateway tai firmware qua HTTP, ghi bang thu vien `Update`, thanh cong thi reboot.
Partition OTA cho gateway duoc cau hinh trong `partitions_gateway_ota.csv`.

### 14.2. Node OTA mo phong qua LoRa

Khi gateway queue command `START_OTA`, node nhan trong ACK va chay phien OTA mo phong:

1. Node gui `OTA_READY`.
2. Gateway gui 10 chunk du lieu firmware gia lap.
3. Node check CRC tung chunk.
4. Node ACK tung chunk.
5. Nhan du chunk thi node gui `OTA_SUCCESS`.

Hien tai day la mo phong giao thuc OTA, chua ghi firmware that vao flash cua node.

## 15. Cach doc log de debug

Node log dung:

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

Gateway log dung:

```text
===== EE4552 GATEWAY FIRMWARE =====
LoRa init OK
Gateway ready
DATA RX node=1 pid=... T=... H=... Soil=... ADC=... V=... RSSI=... Alert=... PumpTime=...
ThingsBoard HTTP status: 200
ACK TX: TYPE=ACK,...
```

Neu can debug nhanh:

- LoRa loi: xem `LoRa init failed`, kiem tra day SPI/nguon/anten.
- DHT loi: `DHT ... ERR=1`, kiem tra DATA GPIO27, pull-up, nguon.
- Soil loi: `SOIL ... ERR=1`, kiem tra AOUT GPIO34 va gia tri ADC.
- Pin sai: kiem tra mach chia ap GPIO35 va `BatteryDividerRatio`.
- Gateway khong len cloud: kiem tra WiFi, token ThingsBoard, `EnableCloudUpload`.

## 16. Gioi han hien tai cua code

- Moi truong mau dang cau hinh `NodeId = 1`, chua co bang quan ly nhieu node day du ngoai mang `lastPacketIdByNode[256]`.
- Dashboard history chi luu RAM 64 mau, khong co database ben vung.
- ThingsBoard gui bang HTTP, chua dung MQTT.
- `PumpTime` la mo phong/goi y. Khong co mach bom trong phan cung hien tai, va khong dau noi gi vao GPIO25.
- Node OTA qua LoRa la mo phong chunk/CRC/status, chua flash firmware that.
- Gateway self-OTA co ghi firmware that qua WiFi.

## 17. Tom tat mot chu ky hoat dong

```text
Node wake up
  -> bat nguon cam bien
  -> doc DHT22
  -> doc cam bien do am dat
  -> do pin
  -> tao TYPE=DATA + CRC
  -> gui LoRa cho Gateway
  -> doi ACK/command
  -> thuc thi command neu co
  -> tinh adaptive sleep
  -> tat LoRa/cam bien
  -> deep sleep

Gateway chay lien tuc
  -> nghe LoRa
  -> nhan TYPE=DATA
  -> check CRC va chong trung packet
  -> luu history RAM
  -> tinh Alert va PumpTime mo phong
  -> upload ThingsBoard
  -> gui TYPE=ACK ve Node
  -> phuc vu dashboard/export/command qua web
```
