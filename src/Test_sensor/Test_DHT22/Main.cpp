#include <Arduino.h>
#include "DHT22_Sensor.h"

DHT22Sensor dht;

const char* statusText(uint8_t status) {
  switch (status) {
    case 0: return "OK";
    case 1: return "ACK fail";
    case 2: return "Timeout read";
    case 3: return "CRC fail";
    case 4: return "Range fail";
    default: return "Unknown";
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("===== DHT22 TEST =====");

  dht.begin();
}

void loop() {
  if (!dht.ready()) return;

  DHT22Data d = dht.read();

  if (d.status == 0) {
    Serial.println("---- DATA ----");

    Serial.print("Status: ");
    Serial.print(d.status);
    Serial.print(" (");
    Serial.print(statusText(d.status));
    Serial.println(")");

    Serial.print("T: ");
    Serial.print(d.temperature, 1);
    Serial.println(" C");

    Serial.print("H: ");
    Serial.print(d.humidity, 1);
    Serial.println(" %");

    Serial.println();
  } else {
    Serial.println("---- ERROR ----");

    Serial.print("Status: ");
    Serial.print(d.status);
    Serial.print(" (");
    Serial.print(statusText(d.status));
    Serial.println(")");

    Serial.println();
  }
}

/*
DHT22 status codes:

0 = OK           → Đọc thành công (ACK, 40 bit, CRC, giá trị hợp lệ)
1 = ACK fail     → Cảm biến không phản hồi (sai dây / chưa cấp nguồn)
2 = Timeout      → Lỗi khi đọc 40 bit (nhiễu / timing sai)
3 = CRC fail     → Dữ liệu sai checksum
4 = Range fail   → Giá trị T/H không hợp lệ hoặc biến động quá lớn
*/