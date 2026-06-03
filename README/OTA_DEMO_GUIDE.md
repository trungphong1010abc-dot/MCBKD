# Quy Trình Demo Và Test Các Tính Năng Cập Nhật/OTA

Dự án: **EE4552 Gateway - Node LoRa**

## Mục Tiêu Demo

Hệ thống có ba cơ chế cập nhật riêng biệt:

1. **Gateway OTA firmware thật qua WiFi/HTTP**
2. **Node cập nhật cấu hình thật qua LoRa và lưu NVS**
3. **Node mô phỏng OTA framework qua LoRa, không flash firmware thật**

Lời dẫn mở đầu:

> Trong hệ thống này, gateway có WiFi nên có thể OTA firmware thật bằng file `.bin` qua HTTP. Node dùng LoRa nên không cập nhật firmware `.bin` thật qua LoRa vì LoRa có băng thông thấp, truyền lâu, tốn pin và dễ mất gói. Vì vậy, node chỉ nhận các cập nhật cấu hình nhỏ qua LoRa và mô phỏng framework OTA để minh họa cơ chế chunk, CRC, ACK/NACK, retry và status.

## 0. Chuẩn Bị Ban Đầu

### 0.1. Kiểm Tra WiFi Gateway

WiFi của gateway được cấu hình trong file `src/project_config.h`:

```cpp
constexpr char WifiSsid[] = "Khoa";
constexpr char WifiPassword[] = "12112004";
```

Laptop phải kết nối cùng mạng WiFi với gateway. Ví dụ gateway dùng WiFi `Khoa` thì laptop cũng phải kết nối WiFi `Khoa`.

Lời dẫn khi quay video:

> Trước khi demo OTA, gateway và laptop phải nằm trong cùng một mạng WiFi. Laptop sẽ đóng vai trò HTTP server chứa file firmware `.bin`, còn gateway sẽ tải file này về để tự cập nhật.

### 0.2. Build, Upload Và Upload And Monitor Là Gì

**Build:** chỉ biên dịch code và tạo file firmware `.bin`. Build không nạp code vào board.

**Upload:** nạp firmware vào board qua USB. Sau khi upload xong, board reset và chạy firmware mới.

**Upload and Monitor:** vừa upload firmware vào board, vừa mở Serial Monitor để xem log sau khi board reset.

Khi nạp firmware lần đầu hoặc muốn kiểm tra board có chạy đúng không, nên dùng:

```text
gateway > General > Upload and Monitor
node    > General > Upload and Monitor
```

Chỉ được rút USB sau khi terminal báo upload thành công, ví dụ:

```text
SUCCESS
```

hoặc sau dòng:

```text
Hard resetting via RTS pin...
```

Không rút USB giữa lúc đang upload vì firmware có thể chưa ghi xong.

Lời dẫn khi quay video:

> Build chỉ tạo file firmware, còn Upload mới nạp firmware vào board. Khi cần xem IP gateway hoặc log LoRa, dùng Upload and Monitor để vừa nạp code vừa theo dõi Serial Monitor.

### 0.3. Nạp Firmware Ban Đầu Cho Gateway Và Node

Nếu dùng PlatformIO UI:

1. Cắm USB vào gateway.
2. Chọn `gateway > General > Upload and Monitor`.
3. Chờ terminal báo `SUCCESS`.
4. Xem log gateway chạy lên.
5. Sau đó có thể rút USB nếu chỉ muốn cấp nguồn ngoài.

Làm tương tự với node:

1. Cắm USB vào node.
2. Chọn `node > General > Upload and Monitor`.
3. Chờ terminal báo `SUCCESS`.
4. Xem node log có `LoRa init OK` và `LoRa TX`.

Nếu dùng PowerShell:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e gateway -t upload
```

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e node -t upload
```

Mở monitor gateway:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -e gateway
```

### 0.4. Nếu LoRa Init Fail Thì Sao

Nếu node log:

```text
LoRa init failed; sleeping 30 min
```

thì node fail LoRa init và đi ngủ để tiết kiệm pin. Đây là đúng thiết kế tiết kiệm năng lượng. Khi demo mà gặp dòng này thì cần kiểm tra phần cứng LoRa của node:

- Nguồn 3.3V cho module LoRa có ổn định không
- GND đã nối chung chưa
- Dây SPI có đúng không
- Anten đã gắn chưa
- Gateway có `LoRa init OK` không

Nếu gateway log:

```text
LoRa init failed; WiFi/web will continue
```

thì gateway vẫn chạy WiFi và dashboard. Lúc này vẫn demo được OTA firmware thật của gateway qua WiFi/HTTP, nhưng không demo được telemetry/config/OTA mô phỏng với node qua LoRa.

## 1. Lấy IP Gateway

Cắm gateway vào laptop bằng USB và mở monitor:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -e gateway
```

Reset gateway. Chờ log:

```text
Connecting WiFi.....
Gateway IP: 192.168.1.7
HTTP server listening on port 80
Gateway ready
```

IP gateway là (IP gateway có thể khác đi sau mỗi lần nạp lại code):

```text
192.168.1.7
```

Mở dashboard:

```text
http://192.168.1.7/
```

Nếu dashboard mở được thì gateway đã kết nối WiFi thành công. Nếu không cắm USB/không xem monitor, có thể lấy IP gateway trong trang quản lý router WiFi, mục danh sách thiết bị đang kết nối.

Lời dẫn khi quay video:

> Sau khi gateway kết nối WiFi, gateway in ra IP trên Serial Monitor. IP này được dùng để mở dashboard, gửi command và gọi endpoint OTA.

## 2. Lấy IP Laptop

Trên laptop, mở PowerShell và chạy:

```powershell
ipconfig
```

Tìm mục:

```text
Wireless LAN adapter Wi-Fi:
```

Lấy dòng:

```text
IPv4 Address . . . . . . . . . . : 192.168.1.12
```

Vậy IP laptop là:

```text
192.168.1.12
```

Gateway và laptop phải cùng lớp mạng. Ví dụ:

```text
Gateway: 192.168.1.7
Laptop : 192.168.1.12
Router : 192.168.1.1
```

Nếu laptop ra IP kiểu `169.254.x.x` hoặc không cùng mạng với gateway thì gateway sẽ không tải được firmware từ laptop.

Lời dẫn khi quay video:

> Laptop IP được dùng trong URL firmware. Gateway sẽ truy cập địa chỉ IP này để tải file `firmware.bin` từ HTTP server đang chạy trên laptop.

## 3. Gateway OTA Firmware Thật Qua WiFi/HTTP

Bản chất: đây là OTA firmware thật. Gateway tải file firmware `.bin` qua WiFi/HTTP, ghi vào OTA partition và reboot sang firmware mới.

```text
Laptop build firmware .bin
Laptop mở HTTP server
Gateway gọi /self_ota
Gateway tải firmware.bin
Gateway ghi OTA partition
Gateway reboot
Gateway chạy version mới
```

Lời dẫn khi quay video:

> Phần này là OTA firmware thật cho gateway. File `.bin` được build từ source code của project. Laptop đóng vai trò server, gateway tải file qua WiFi, ghi vào OTA partition và reboot sang bản firmware mới.

### 3.1. Đổi Version Để Chứng Minh OTA

Mở file:

```text
src/project_config.h
```

Đổi version, ví dụ:

```cpp
constexpr char GatewayFirmwareVersion[] = "GW_OTA_TEST_4";
```

Lần sau test thì tăng version:

```text
GW_OTA_TEST_5
GW_OTA_TEST_6
```

Mục đích: sau OTA, nếu dashboard/monitor hiện version mới thì chứng minh gateway đã chạy firmware mới.

### 3.2. Build Firmware Gateway Mới

```powershell
cd D:\Code_Project\Code_MCBKD
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e gateway
```

Hoặc trong PlatformIO UI:

```text
gateway > General > Build
```

Sau khi build, file firmware sinh ra tại:

```text
D:\Code_Project\Code_MCBKD\.pio\build\gateway\firmware.bin
```

### 3.3. Mở HTTP Server Trên Laptop

Dùng lệnh có `--directory` để tránh sai thư mục:

```powershell
python -m http.server 8000 --directory D:\Code_Project\Code_MCBKD\.pio\build\gateway
```

Nếu `python` lỗi, dùng:

```powershell
py -m http.server 8000 --directory D:\Code_Project\Code_MCBKD\.pio\build\gateway
```

Giữ terminal này mở. Nếu tắt terminal thì gateway không tải được firmware.

### 3.4. Test File Firmware Trên Browser Laptop

```text
http://192.168.1.12:8000/firmware.bin
```

Nếu browser tải được file `firmware.bin` thì đúng.

Nếu bị `404 File not found`, thường do:

- Chưa build gateway
- Server đang trỏ sai thư mục
- Chưa có file `.pio\build\gateway\firmware.bin`

### 3.5. Gọi OTA Gateway

```text
http://192.168.1.7/self_ota?url=http://192.168.1.12:8000/firmware.bin
```

Trong đó:

```text
192.168.1.7  = IP gateway
192.168.1.12 = IP laptop
8000         = port HTTP server trên laptop
```

Browser sẽ hiện:

```text
OTA started; gateway will reboot on success
```

Dòng này có nghĩa gateway đã nhận yêu cầu OTA hợp lệ và bắt đầu tải firmware. Đây chưa phải bằng chứng cuối cùng, cần chờ gateway reboot và kiểm tra version mới.

### 3.6. Kiểm Tra OTA Thành Công

Sau khi gateway reboot, mở dashboard:

```text
http://192.168.1.7/
```

Hoặc xem monitor gateway:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -e gateway
```

Thành công khi thấy:

```text
Firmware: GW_OTA_TEST_4
Gateway ready
```

Lời dẫn khi quay video:

> Sau khi OTA, gateway reboot và hiện version mới. Việc version thay đổi chứng minh gateway đã OTA firmware thật thành công, không cần nạp lại bằng USB.

## 4. Node Cập Nhật Cấu Hình Thật Qua LoRa Và Lưu NVS

Bản chất: đây không phải OTA firmware. Đây là cập nhật cấu hình thật cho node, ví dụ ngưỡng độ ẩm đất, thời gian bơm, chế độ lọc, chu kỳ ngủ. Dữ liệu cấu hình nhỏ nên phù hợp với LoRa.

```text
Browser/Laptop --WiFi/HTTP--> Gateway --LoRa ACK command--> Node --NVS--> Lưu cấu hình
```

Người dùng gửi command đến gateway qua WiFi/HTTP. Gateway queue command. Khi node gửi telemetry qua LoRa, gateway chèn command vào gói ACK trả về. Node nhận command, cập nhật runtime config và lưu vào NVS.

Lời dẫn khi quay video:

> Phần này là cập nhật cấu hình thật cho node. Command ban đầu đi từ browser đến gateway qua WiFi/HTTP, sau đó gateway gửi xuống node bằng LoRa. Node lưu vào NVS nên sau reset hoặc mất nguồn cấu hình vẫn còn.

### 4.1. Mở Dashboard Gateway

```text
http://192.168.1.7/
```

### 4.2. Gửi Lệnh Đổi Ngưỡng Độ Ẩm Đất

```text
http://192.168.1.7/command?node=1&cmd=SET_THRESHOLD&param=45
```

Browser hiện:

```text
queued
```

Gateway log:

```text
Queued HTTP command node=1 cmd=SET_THRESHOLD param=45
```

### 4.3. Cho Node Nhận Command

Command không được gateway gửi riêng ngay lập tức. Gateway đợi node gửi telemetry lần tiếp theo, rồi chèn command vào ACK. Để node nhận ngay, bấm reset node.

Gateway log đúng:

```text
DATA RX node=1 ...
ACK TX: TYPE=ACK,NODE=1,...,CMD=SET_THRESHOLD,PARAM=45,STATUS=COMMAND,...
```

### 4.4. Kiểm Tra Config Đã Cập Nhật

Sau lần telemetry tiếp theo, dashboard sẽ hiện giá trị config mới của node.

Có thể test thêm thời gian bơm:

```text
http://192.168.1.7/command?node=1&cmd=SET_PUMP_TIME&param=12
```

Sau đó reset node và quan sát log:

```text
ACK TX: ... CMD=SET_PUMP_TIME,PARAM=12,STATUS=COMMAND ...
```

Lời dẫn khi quay video:

> Gateway không gửi command riêng lẻ ngay, mà gửi kèm trong ACK khi node thức dậy và gửi telemetry. Cách này phù hợp với node tiết kiệm pin vì node không phải nghe LoRa liên tục.

Kết luận mục này:

> Đây là cập nhật cấu hình thật qua LoRa, không phải cập nhật firmware. Các tham số nhỏ được truyền qua LoRa và lưu NVS nên node giữ cấu hình sau khi reset.

## 5. Node Mô Phỏng OTA Framework Qua LoRa, Không Flash Firmware Thật

Bản chất: đây là mô phỏng quy trình OTA cho node qua LoRa. Node không update firmware `.bin` thật. Node chỉ nhận các chunk giả lập firmware, kiểm CRC, gửi ACK/NACK, gateway retry và báo status.

Node không ghi vào flash, không dùng `Update.write`, không đổi firmware đang chạy.

Lý do không OTA firmware thật qua LoRa:

- Firmware `.bin` thường lớn
- LoRa băng thông thấp
- Truyền lâu
- Tốn pin
- Dễ mất gói
- Cần cơ chế retry/đồng bộ rất chặt

Lời dẫn khi quay video:

> Phần này chỉ mô phỏng framework OTA cho node. Gateway gửi các chunk giả lập firmware qua LoRa, node kiểm CRC và trả ACK/NACK. Node không ghi firmware vào flash nên đây không phải update firmware thật.

### 5.1. Gọi Lệnh START_OTA

```text
http://192.168.1.7/command?node=1&cmd=START_OTA&param=13270
```

Browser hiện:

```text
queued
```

Gateway log:

```text
Queued HTTP command node=1 cmd=START_OTA param=13270
```

### 5.2. Cho Node Nhận START_OTA

Reset node hoặc chờ node gửi telemetry tiếp theo. Gateway sẽ gửi START_OTA trong ACK:

```text
ACK TX: TYPE=ACK,NODE=1,...,CMD=START_OTA,PARAM=13270,STATUS=COMMAND,...
```

### 5.3. Quan Sát Log OTA Mô Phỏng

Log khi bắt đầu:

```text
OTA waiting for OTA_READY node=1 ota_id=13270
OTA STATUS RX node=1 ota_id=13270 idx=0 ok=1 status=OTA_READY
OTA session start node=1 ota_id=13270
OTA CHUNK TX idx=1 ...
```

Nếu truyền ổn:

```text
OTA CHUNK TX idx=2 ...
OTA CHUNK TX idx=3 ...
...
OTA session success node=1 ota_id=13270 chunks=10
```

Nếu fail giữa chừng, ví dụ:

```text
OTA status unexpected: OTA_FAILED_TIMEOUT
OTA session failed at chunk 6
```

Giải thích: phiên OTA mô phỏng đã bắt đầu và truyền được một số chunk. Tại chunk số 6, node không nhận được chunk hợp lệ trong thời gian chờ. Gateway retry nhiều lần nhưng không nhận ACK đúng nên kết thúc phiên. Đây là hành vi an toàn của framework OTA mô phỏng khi đường truyền LoRa mất gói hoặc timeout.

Lời dẫn khi quay video:

> Đây không phải OTA firmware thật thất bại. Node không ghi dữ liệu vào flash và không thay đổi chương trình đang chạy. Dữ liệu gửi qua LoRa chỉ là các chunk giả lập firmware để minh họa cơ chế chunk, CRC, ACK/NACK, retry và status.

Kết luận mục này:

> Thử nghiệm này cho thấy nếu muốn OTA firmware thật qua LoRa thì phải có cơ chế đồng bộ và retry rất chặt, trong khi thời gian truyền và năng lượng tiêu thụ cao. Vì vậy hệ thống chỉ dùng LoRa cho telemetry, command cấu hình nhỏ và mô phỏng OTA; firmware thật chỉ OTA trên gateway qua WiFi/HTTP.

## 6. Tổng Kết Cuối Video

Có thể nói:

> Tổng kết lại, hệ thống có ba cơ chế cập nhật. Thứ nhất, gateway OTA firmware thật qua WiFi/HTTP bằng file `.bin` và reboot sang version mới. Thứ hai, node nhận cập nhật cấu hình thật qua LoRa và lưu vào NVS để giữ sau khi reset hoặc mất nguồn. Thứ ba, OTA firmware cho node qua LoRa chỉ được mô phỏng bằng chunk, CRC, ACK/NACK, retry và status, không flash firmware thật. Cách tách này phù hợp với thực tế: gateway có WiFi nên OTA firmware thật, còn node dùng LoRa nên chỉ truyền telemetry và command cấu hình nhỏ để tiết kiệm pin và tránh rủi ro.
