#include <Arduino.h>
#include "DHT22_Sensor.h"

DHT22Sensor dht;

const char* statusText(DHT22Status status) {
  if (status == DHT22_STATUS_OK) return "OK";
  return "ERROR";
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("===== DHT22 TEST =====");

  dht.begin(); // Khởi tạo DHT22
}

void loop() {
  if (!dht.ready()) {
    return; // Chưa đến chu kỳ đo thì thoát loop
  }

  DHT22Data data = dht.read();

  if (data.status == DHT22_STATUS_OK) {
    Serial.println("---- DATA ----");

    Serial.print("DHT22_status: ");
    Serial.println(statusText(data.status));

    Serial.print("T_air: ");
    Serial.print(data.T_air, 1);
    Serial.println(" C");

    Serial.print("H_air: ");
    Serial.print(data.H_air, 1);
    Serial.println(" %RH");

    Serial.println();
  } else {
    Serial.println("---- ERROR ----");

    Serial.print("DHT22_status: ");
    Serial.println(statusText(data.status));

    Serial.println("DHT22 read/range failed");
    Serial.println("Bo mau hien tai, retry o chu ky sau");

    Serial.println();
  }

  delay(2000); // Delay chu kỳ đo
}