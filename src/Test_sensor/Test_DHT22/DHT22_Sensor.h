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

// ================= STATUS =================
enum DHT22Status {
  DHT22_STATUS_OK,
  DHT22_STATUS_ERROR
};

// ================= DATA =================
struct DHT22Data {
  float T_air;
  float H_air;
  DHT22Status status;
};

class DHT22Sensor {
private:
  uint8_t pin;

  float T_buffer[FILTER_SIZE];
  float H_buffer[FILTER_SIZE];

  uint8_t index = 0;
  uint8_t count = 0;

  float T_prev = NAN;
  unsigned long lastRead = 0;

private:
  bool waitLevel(uint8_t level) {
    uint32_t start = micros();

    while (digitalRead(pin) != level) {
      if (micros() - start > TIMEOUT_US) return false; // Timeout khi chờ mức tín hiệu
    }

    return true;
  }

  uint32_t readHighTime() {
    uint32_t start = micros();

    while (digitalRead(pin) == LOW) {
      if (micros() - start > TIMEOUT_US) return 0;
    }

    uint32_t highStart = micros();

    while (digitalRead(pin) == HIGH) {
      if (micros() - highStart > TIMEOUT_US) return 0;
    }

    return micros() - highStart; // Độ rộng xung HIGH dùng để xác định bit 0/1
  }

  void pushBuffer(float T, float H) {
    T_buffer[index] = T;
    H_buffer[index] = H;

    index = (index + 1) % FILTER_SIZE;

    if (count < FILTER_SIZE) count++;
  }

  float average(float *buffer, uint8_t n) {
    float sum = 0;

    for (uint8_t i = 0; i < n; i++) {
      sum += buffer[i];
    }

    return sum / n;
  }

  float round01(float value) {
    return roundf(value * 10.0) / 10.0; // Làm tròn đến 0.1
  }

  DHT22Data makeError() {
    DHT22Data data;
    data.T_air = NAN;
    data.H_air = NAN;
    data.status = DHT22_STATUS_ERROR;
    return data;
  }

public:
  DHT22Sensor(uint8_t p = DHT22_PIN) {
    pin = p;
  }

  void begin() {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH); // Bus DATA ở trạng thái HIGH ban đầu

    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
      T_buffer[i] = 0;
      H_buffer[i] = 0;
    }

    index = 0;
    count = 0;
    T_prev = NAN;
    lastRead = 0;

    delay(2000); // Chờ DHT22 ổn định
  }

  bool ready() {
    return millis() - lastRead >= READ_INTERVAL;
  }

  DHT22Data read() {
    if (!ready()) {
      return makeError();
    }

    lastRead = millis();

    uint8_t B[5] = {0};

    // ================= START SIGNAL =================
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(10);

    digitalWrite(pin, LOW);
    delay(18); // MCU kéo LOW 18ms để gửi tín hiệu START

    digitalWrite(pin, HIGH);
    delayMicroseconds(30);

    pinMode(pin, INPUT_PULLUP); // Chuyển sang INPUT để chờ DHT22 phản hồi

    // ================= ACK =================
    if (!waitLevel(LOW) || !waitLevel(HIGH) || !waitLevel(LOW)) {
      return makeError();
    }

    // ================= READ 40 BIT / 5 BYTE =================
    for (uint8_t i = 0; i < 40; i++) {
      uint32_t highTime = readHighTime();

      if (highTime == 0) {
        return makeError();
      }

      B[i / 8] <<= 1;

      if (highTime > 40) {
        B[i / 8] |= 1; // Xung HIGH dài hơn 40us thì xem là bit 1
      }
    }

    uint8_t humH = B[0]; // Hum_H
    uint8_t humL = B[1]; // Hum_L
    uint8_t tempH = B[2]; // Temp_H
    uint8_t tempL = B[3]; // Temp_L
    uint8_t crc = B[4]; // CRC

    // ================= CRC CHECK =================
    uint8_t checksum = (humH + humL + tempH + tempL) & 0xFF;

    if (checksum != crc) {
      return makeError();
    }

    // ================= TÍNH NHIỆT ĐỘ =================
    uint16_t rawT = ((tempH & 0x7F) << 8) | tempL;
    float T_air = rawT / 10.0;

    if (tempH & 0x80) {
      T_air = -T_air;
    }

    // ================= TÍNH ĐỘ ẨM =================
    float H_air = ((humH << 8) | humL) / 10.0;

    // ================= OFFSET HIỆU CHỈNH =================
    T_air = T_air + TEMP_OFFSET;
    H_air = H_air + HUM_OFFSET;

    // ================= KIỂM TRA DỮ LIỆU HỢP LỆ =================
    if (T_air < TEMP_MIN || T_air > TEMP_MAX) {
      return makeError();
    }

    if (H_air < HUM_MIN || H_air > HUM_MAX) {
      return makeError();
    }

    if (!isnan(T_prev)) {
      if (fabs(T_air - T_prev) > TEMP_DELTA_MAX) {
        return makeError();
      }
    }

    // ================= MOVING AVERAGE N = 5 =================
    pushBuffer(T_air, H_air);

    float T_flt = average(T_buffer, count);
    float H_flt = average(H_buffer, count);

    T_flt = round01(T_flt);
    H_flt = round01(H_flt);

    T_prev = T_flt;

    // ================= LƯU / XUẤT DATA =================
    DHT22Data data;
    data.T_air = T_flt;
    data.H_air = H_flt;
    data.status = DHT22_STATUS_OK;

    return data;
  }
};

#endif