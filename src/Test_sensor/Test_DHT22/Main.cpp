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

*/