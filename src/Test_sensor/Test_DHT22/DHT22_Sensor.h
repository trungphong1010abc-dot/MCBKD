#ifndef DHT22_SENSOR_H
#define DHT22_SENSOR_H

#include <Arduino.h>
#include <math.h>

// ================= CONFIG =================
#define DHT22_PIN 14

#define READ_INTERVAL 2000
#define TIMEOUT_US 250

#define FILTER_SIZE 5

#define TEMP_MIN -10.0
#define TEMP_MAX 50.0
#define HUM_MIN 0.0
#define HUM_MAX 100.0

#define TEMP_OFFSET 0.0
#define HUM_OFFSET 0.0

#define TEMP_DELTA_MAX 1.0

// ================= DATA =================
struct DHT22Data {
  float temperature;
  float humidity;
  uint8_t status;
};

class DHT22Sensor {
private:
  uint8_t pin;

  float tempBuf[FILTER_SIZE];
  float humBuf[FILTER_SIZE];

  uint8_t idx = 0;
  uint8_t count = 0;

  float T_prev = NAN;

  unsigned long lastRead = 0;

private:
  bool waitLevel(uint8_t level) {
    uint32_t t = micros();
    while (digitalRead(pin) != level) {
      if (micros() - t > TIMEOUT_US) return false;
    }
    return true;
  }

  uint32_t readHighTime() {
    uint32_t t = micros();

    while (digitalRead(pin) == LOW) {
      if (micros() - t > TIMEOUT_US) return 0;
    }

    uint32_t start = micros();

    while (digitalRead(pin) == HIGH) {
      if (micros() - start > TIMEOUT_US) return 0;
    }

    return micros() - start;
  }

  float avg(float *buf, uint8_t n) {
    float s = 0;
    for (int i = 0; i < n; i++) s += buf[i];
    return s / n;
  }

  void push(float T, float H) {
    tempBuf[idx] = T;
    humBuf[idx] = H;

    idx = (idx + 1) % FILTER_SIZE;
    if (count < FILTER_SIZE) count++;
  }

public:
  DHT22Sensor(uint8_t p = DHT22_PIN) {
    pin = p;
  }

  void begin() {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);

    for (int i = 0; i < FILTER_SIZE; i++) {
      tempBuf[i] = 0;
      humBuf[i] = 0;
    }

    lastRead = 0;
    T_prev = NAN;

    delay(2000); // ổn định cảm biến
  }

  bool ready() {
    return millis() - lastRead >= READ_INTERVAL;
  }

  DHT22Data read() {
    DHT22Data out;
    out.status = 2;

    if (!ready()) return out;

    lastRead = millis();

    uint8_t data[5] = {0};

    // ================= START SIGNAL =================
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(10);

    digitalWrite(pin, LOW);
    delay(18);

    digitalWrite(pin, HIGH);
    delayMicroseconds(30);
    pinMode(pin, INPUT_PULLUP);

    // ================= ACK =================
    if (!waitLevel(LOW) || !waitLevel(HIGH) || !waitLevel(LOW)) {
      out.status = 1;
      return out;
    }

    // ================= READ 40 BIT =================
    for (int i = 0; i < 40; i++) {
      uint32_t t = readHighTime();
      if (t == 0) {
        out.status = 2;
        return out;
      }

      data[i / 8] <<= 1;
      if (t > 40) data[i / 8] |= 1;
    }

    uint8_t b0 = data[0];
    uint8_t b1 = data[1];
    uint8_t b2 = data[2];
    uint8_t b3 = data[3];
    uint8_t b4 = data[4];

    // ================= CRC =================
    uint8_t sum = (b0 + b1 + b2 + b3) & 0xFF;
    if (sum != b4) {
      out.status = 3;
      return out;
    }

    // ================= T =================
    uint16_t rawT = ((b2 & 0x7F) << 8) | b3;
    float T = rawT / 10.0;

    if (b2 & 0x80) T = -T;

    // ================= H =================
    float H = ((b0 << 8) | b1) / 10.0;

    // ================= OFFSET =================
    T += TEMP_OFFSET;
    H += HUM_OFFSET;

    // ================= RANGE =================
    if (T < TEMP_MIN || T > TEMP_MAX) {
      out.status = 4;
      return out;
    }

    if (H < HUM_MIN || H > HUM_MAX) {
      out.status = 4;
      return out;
    }

    if (!isnan(T_prev)) {
      if (fabs(T - T_prev) > TEMP_DELTA_MAX) {
        out.status = 4;
        return out;
      }
    }

    // ================= FILTER =================
    push(T, H);

    float Tf = avg(tempBuf, count);
    float Hf = avg(humBuf, count);

    T_prev = Tf;

    out.temperature = Tf;
    out.humidity = Hf;
    out.status = 0;

    return out;
  }
};

#endif