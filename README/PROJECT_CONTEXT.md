# Project Context - EE4552_2025

Nguon tham chieu goc:

- Word: `d:\OneDrive\Tai_lieu_cac_mon\Mang_CB_ko_day\Yeu cau cho du an mon hoc EE4552_2025.docx`
- 9 flowchart trong conversation: Soil Moisture, DHT22, Gateway -> Node, Node -> Gateway, Gateway -> ThingsBoard IoT Application, Adaptive Duty Cycle, Gateway-managed Node OTA over LoRa, Gateway Self-OTA over WiFi, Main flow.

## De tai

Thiet ke mang cam bien khong day giam sat nhiet do va do am tren cac canh dong.

## Yeu cau tu file Word

- Do luong:
  - Nhiet do: -10 C den 50 C.
  - Do am khong khi: 0 den 100 %RH.
  - Do am dat: 0 den 60 %Vol.
  - Do chinh xac muc tieu: +/-0.5 C, +/-2 %RH, +/-3 %Vol.
  - Do phan giai hien thi: 0.1 C, 0.1 %RH.
  - Thoi gian do mot mau: <60 s, muc nang cao <10 s.
- Nguon:
  - Node dung pin, uu tien tiet kiem nang luong.
  - Tuoi tho pin/node: >= 6 thang voi che do tiet kiem nang luong.
  - Co the thay pin, sac lai bang nang luong tai tao.
- Co khi:
  - Kich thuoc du kien: 50 x 50 x 80 mm.
  - Trong luong du kien: <150 g.
  - Huong toi vo chong nuoc IP68.
- Truyen thong:
  - Cong nghe RF uu tien vung phu rong.
  - Ban kinh truyen nhan moi node: >=100 m.
  - Gateway ket noi Internet qua WiFi, 4G hoac Ethernet.
  - Do tre thu thap du lieu: <30 s, muc nang cao <5 s.
  - Quan ly toi thieu 100 node, muc nang cao hang nghin node.
- Phan mem va giao dien:
  - Thu thap gia tri do tu thiet bi.
  - Quan ly du lieu.
  - Xuat bao cao Excel.
  - Giao dien theo mau thong nhat.
- Mo rong:
  - OTA firmware.
  - Adaptive duty cycle.
  - Phan tich du lieu nong nghiep chinh xac, vi du du bao/ra quyet dinh tuoi tieu.

## Tom tat 9 flowchart

### 1. Main flow

- Khoi tao ESP32, GPIO, ADC, LoRa, RTC memory va dieu khien nguon cam bien.
- Wake-up tu Deep Sleep, doc nguyen nhan wake-up va RTC timer.
- Bat nguon cam bien bang MOSFET AO3400, delay on dinh.
- Goi DHT22 flow de lay `T_air`, `H_air`, `Error_Flag`.
- Goi Soil Moisture flow de lay `H_soil`, `Soil_Status`, `ADC_filtered`.
- Neu du lieu loi thi tao canh bao `SENSOR_ERROR`.
- Dong goi packet LoRa: `Node_ID`, `Packet_ID`, `T_air`, `H_air`, `H_soil`, `V_bat`, `Error_Flag`, `CRC`.
- Goi Node -> Gateway flow.
- Goi Application flow tren Gateway.
- Neu co command tu Gateway thi goi Gateway -> Node flow.
- Goi Adaptive Duty Cycle flow de tinh `Sleep_Duration`.
- Luu `Sleep_Duration` vao RTC memory, tat nguon cam bien, vao Deep Sleep.

### 2. DHT22 flow

- Khoi tao ESP32, Serial Debug va GPIO14 cho DHT22.
- Dung single-bus timing protocol:
  - DATA mac dinh HIGH.
  - ESP32 keo LOW >=18 ms de gui START.
  - Chuyen GPIO sang INPUT va cho DHT22 phan hoi.
- Doc 40 bit/5 byte: `Hum_H`, `Hum_L`, `Temp_H`, `Temp_L`, `CRC`.
- Kiem tra ACK va CRC: checksum = `(B0 + B1 + B2 + B3) & 0xFF`, so sanh voi `B4`.
- Tinh nhiet do, gom ca bit dau am.
- Tinh do am: `H = ((B0 << 8) | B1) / 10.0`.
- Ap dung offset hieu chinh `TEMP_OFFSET`, `RH_OFFSET`.
- Kiem tra hop le:
  - -10 C <= `T_air` <= 50 C.
  - 0 %RH <= `H_air` <= 100 %RH.
  - Bien thien moi chu ky hop ly: `|T_air - T_prev| <= 2 C`, `|H_air - H_prev| <= 5 %RH`.
- Loc moving average N=5, lam tron den 0.1 C va 0.1 %RH.
- Loi thi set `DHT22_status = ERROR`, `Error_Flag = SENSOR_ERROR`, bo mau loi va giu chu ky tiep theo.

### 3. Soil Moisture flow

- Khoi tao ADC1 GPIO32-39, 12 bit, attenuation 11 dB va cau hinh kenh cam bien.
- Delay on dinh cam bien.
- Doc ADC tho va kiem tra hop le: `ADC_min < ADC < ADC_max`.
- Neu hop le:
  - Reset bo dem loi ADC.
  - Thu thap N mau ADC, bo 3 mau dau on dinh cam bien.
  - Loc nhieu median filter thanh `ADC_filtered`.
  - Tinh `H_soil = (ADC_dry - ADC_filtered) * 100 / (ADC_dry - ADC_wet)`.
  - Gioi han `H_soil` trong 0 den 60 %Vol.
  - Cap nhat `SoilData`: `H_soil`, `ADC_filtered`, `Soil_status`, `Error_Flag`.
- Neu ADC loi:
  - Tang bo dem loi.
  - Retry ngan 300-500 ms neu chua vuot nguong.
  - Neu loi qua nguong thi set `Error_Flag = SENSOR_ERROR`, `Soil_status = ERROR`, bo mau hien tai.

### 4. Node -> Gateway flow

- Node wake-up tu Deep Sleep bang RTC timer, doc DHT22, do am dat va pin trong <10 s.
- Kiem tra du lieu cam bien hop le:
  - -10 C <= `T_air` <= 50 C.
  - 0 %RH <= `H_air` <= 100 %RH.
  - 0 %Vol <= `H_soil` <= 60 %Vol.
- Dong goi LoRa: `Header`, `Node_ID`, `Packet_ID`, `T_air`, `H_air`, `H_soil`, `V_bat`, `Error_Flag`, `CRC`.
- Khoi tao LoRa TX voi tan so, cong suat va spreading factor; muc tieu RF range >=100 m.
- Gui packet RF den Gateway.
- Gateway nhan, kiem tra CRC/header, Node_ID hop le va chong trung `Packet_ID`.
- Gateway parse du lieu, luu local/database, hien thi dashboard, forward telemetry len cloud/ThingsBoard/MQTT, co the export lich su Excel.
- Gateway tao ACK packet: `Node_ID`, `Packet_ID`, `Status`, `CRC` va gui ve Node.
- Node cho ACK/command trong `T_ACK`, xac thuc `Node_ID`, `Packet_ID`, `CRC`.
- Neu ACK loi thi retry den `MAX_RETRY`; neu van loi thi luu data buffer gui lai sau.
- Node vao Deep Sleep theo adaptive duty cycle.

### 5. Gateway -> Node command flow

- Gateway nhan/lua command tu phan mem:
  - `SET_SLEEP_DURATION`
  - `SET_THRESHOLD`
  - `START_OTA`
  - `SLEEP_NOW`
  - `START_PUMP`
- Kiem tra `Node_ID` hop le.
- Dong goi packet LoRa command: `Header`, `Gateway_ID`, `Node_ID`, `Command`, `Parameter`, `Packet_ID`, `Timestamp`, `CRC`.
- Gui RF den Node, doi ACK trong `T_ACK`.
- Node nhan command, kiem tra CRC/header, dung Node_ID va chong trung Packet_ID.
- Node phan loai va thuc thi command, luu cau hinh vao Flash/EEPROM neu can.
- Node tao ACK: `Node_ID`, `Packet_ID`, `Status`, `CRC`.
- Gateway xac thuc ACK, cap nhat `COMMAND_SUCCESS` hoac command failed.

### 6. Gateway -> ThingsBoard IoT Application flow

- Gateway nhan du lieu node, kiem tra packet/CRC/Node_ID/Error_Flag.
- Loi thi hien thi canh bao `Sensor error`, `RF error`, `Node offline`.
- Du lieu hop le thi luu database, hien thi dashboard realtime: `T_air`, `H_air`, `H_soil`, `V_bat`, `RSSI`.
- Ket noi ThingsBoard qua WiFi/Ethernet/4G.
- Neu mat cloud thi luu local buffer `CLOUD_OFFLINE`, gui lai khi reconnect.
- Gui telemetry bang MQTT/HTTP gom `Node_ID`, `T_air`, `H_air`, `H_soil`, `V_bat`, `RSSI`.
- Phan loai do am dat va tinh `pumpTime`:
  - `H_soil > 42`: `OVER_MOISTURE`, `pumpTime = 0`.
  - `33 < H_soil <= 42`: `NORMAL`, `pumpTime = 0`.
  - `24 < H_soil <= 33`: `LIGHT_DRY`, `pumpTime = 3`.
  - `15 < H_soil <= 24`: `NEED_WATERING`, `pumpTime = 6`.
  - `H_soil <= 15`: `URGENT_WATERING`, `pumpTime = 10`.
- Hieu chinh theo moi truong:
  - Neu `T_air > 32 C`: `pumpTime += 2`.
  - Neu `H_air < 50 %RH`: `pumpTime += 3`.
  - Neu `H_air > 80 %RH`: `pumpTime -= 2`.
  - Gioi han `0 <= pumpTime <= 15`.
- Tao command tuoi `START_PUMP` gui ve Node va cap nhat dashboard.
- Ho tro xuat bao cao Excel `.xlsx` theo `Node_ID` va khoang thoi gian.

### 7. Adaptive Duty Cycle flow

- Input: `Soil_Status`, `H_soil`, `V_bat`, `PumpTime`, `Error_Flag`.
- Neu `Error_Flag != 0`: `Sleep_Duration = 30` phut va gui canh bao loi.
- Neu pin rat yeu `V_bat < 3.3 V`: `Sleep_Duration = 90` phut, `Battery_Status = CRITICAL`, canh bao uu tien, thong bao, khoa OTA.
- Neu pin yeu `V_bat < 3.5 V`: `Sleep_Duration = 60` phut, `Battery_Status = LOW`, canh bao, thong bao, khoa OTA.
- Neu du lieu hop le, phan loai `Soil_Status`:
  - `URGENT_WATERING`: 5 phut.
  - `NEED_WATERING`: 10 phut.
  - `LIGHT_DRY`: 20 phut.
  - `NORMAL`: 30 phut.
  - `OVER_MOISTURE`: 60 phut.
- Gioi han `5 <= Sleep_Duration <= 90` phut.
- Luu `Sleep_Duration` vao RTC memory va vao Deep Sleep bang `esp_sleep_enable_timer_wakeup()` / `esp_deep_sleep_start()`.

### 8. Gateway-managed Node OTA over LoRa

- Gateway nhan firmware moi: file, version, size, `CRC_total`.
- Gateway gui command `START_OTA` den Node kem `Node_ID`, `OTA_VERSION`, `FW_SIZE`, `CRC_total`.
- Node dong y OTA thi tao OTA partition/flash region, clear OTA buffer.
- Gateway chia firmware thanh chunk: `Chunk_ID`, `Chunk_Data`, `Chunk_CRC`.
- Gateway gui tung OTA chunk qua LoRa.
- Node kiem tra CRC tung chunk:
  - Dung thi ghi chunk vao Flash va ACK chunk ve Gateway.
  - Sai thi yeu cau gui lai `Resend_Chunk_ID`.
- Khi nhan du firmware, Node kiem tra CRC toan bo.
- Neu dung thi reboot chay firmware moi, Gateway cap nhat `OTA_SUCCESS`, firmware version moi, node online.
- Neu sai/that bai thi rollback firmware cu, set `Node_status = ERROR`.

### 9. Gateway Self-OTA over WiFi

- Gateway nhan OTA command tu ThingsBoard.
- So sanh firmware version local vs cloud.
- Neu co firmware moi thi download `.bin` tu HTTP server/cloud.
- Kiem tra CRC/checksum firmware.
- CRC hop le thi ghi firmware vao ESP32 OTA partition, reboot Gateway, chay firmware moi, report `OTA_SUCCESS` ve ThingsBoard.
- CRC loi thi report `OTA_FAILED` ve ThingsBoard va ket thuc OTA.

## Nguyen tac code sau nay

- Tach ro node firmware va gateway firmware; khong tron flow cam bien voi flow cloud/dashboard.
- Moi packet RF/command/ACK/OTA phai co `Node_ID`, `Packet_ID` hoac `Chunk_ID`, `CRC` va co logic chong trung/retry.
- Moi du lieu cam bien phai co validate, filter, status va `Error_Flag`.
- Cac nguong do am dat, pin, retry, timeout, calibration ADC dry/wet nen dua vao constant/config de de hieu chinh.
- Khi implement tinh nang moi, doi chieu file nay truoc de giu dung flow da thiet ke.
