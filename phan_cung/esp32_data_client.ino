#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"  // thư viện MAX30105 tương thích với MAX30102

#define SDA_PIN 21
#define SCL_PIN 22

// Thông tin Wi-Fi
const char* ssid     = "newton";
const char* password = "newtonewton";

// Địa chỉ server UDP
const char* SERVER_IP   = "192.168.224.230";
const uint16_t SERVER_PORT = 5005;

// Khai báo thư viện
WiFiUDP udp;
Adafruit_MPU6050 mpu;
MAX30105 particleSensor;

// Để đảm bảo 50 Hz → 20 ms
unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);
  // 1) Kết nối Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected, IP = " + WiFi.localIP().toString());

  // 2) Khởi tạo I2C cho MPU6050 & MAX30102
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found!");
    while (1) delay(10);
  }
  particleSensor.setup(0x1F, 1, 2, 400, 411, 4096);
  particleSensor.clearFIFO();

  // 3) Khởi UDP (local port bất kỳ, ví dụ  localPort=1234)
  udp.begin(1234);
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleTime >= 20) {       // 20 ms → 50 Hz
    lastSampleTime = now;

    // 1) Đọc cảm biến
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    uint32_t irValue = particleSensor.getIR();

    // 2) Tạo chuỗi CSV: "ESP_ms,accX,accY,accZ,gyroX,gyroY,gyroZ,ir"
    String line = String(now);
    line += "," + String(a.acceleration.x, 3)
          + "," + String(a.acceleration.y, 3)
          + "," + String(a.acceleration.z, 3)
          + "," + String(g.gyro.x, 3)
          + "," + String(g.gyro.y, 3)
          + "," + String(g.gyro.z, 3)
          + "," + String(irValue);

    // 3) Gửi UDP
    udp.beginPacket(SERVER_IP, SERVER_PORT);
    udp.print(line);
    udp.endPacket();

    // 4) Debug qua Serial
    Serial.println("Sent: " + line);
  }
}
