#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "svm_model.h"
#include "scaler_values.h"

#define RED_PIN   4
#define GREEN_PIN 2
#define BLUE_PIN  15

#define SDA_PIN 21
#define SCL_PIN 22
#define WINDOW_SIZE 50
#define N_RAW_FEATURES 7

Adafruit_MPU6050 mpu;
MAX30105 particleSensor;

float buffer[WINDOW_SIZE][N_RAW_FEATURES];
int buffer_index = 0;

// SVM prediction
int predict(float* features) {
    float norm_features[n_features];
    norm_features[0] = (features[0] - IR_mean_min) / (IR_mean_max - IR_mean_min);
    norm_features[1] = (features[1] - AccelX_std_min) / (AccelX_std_max - AccelX_std_min);
    norm_features[2] = (features[2] - AccelY_std_min) / (AccelY_std_max - AccelY_std_min);
    norm_features[3] = (features[3] - AccelZ_std_min) / (AccelZ_std_max - AccelZ_std_min);
    norm_features[4] = (features[4] - GyroX_std_min) / (GyroX_std_max - GyroX_std_min);
    norm_features[5] = (features[5] - GyroY_std_min) / (GyroY_std_max - GyroY_std_min);
    norm_features[6] = (features[6] - GyroZ_std_min) / (GyroZ_std_max - GyroZ_std_min);

    float decision = 0.0;
    for (int i = 0; i < n_support_vectors; i++) {
        float dot_product = 0.0;
        for (int j = 0; j < n_features; j++) {
            dot_product += norm_features[j] * support_vectors[i * n_features + j];
        }
        decision += dual_coef[i] * dot_product;
    }
    decision += intercept;
    return decision > 0 ? 1 : 0;
}

// Feature extraction
void extract_features(float* features_out) {
    float ir_sum = 0;
    float means[N_RAW_FEATURES] = {0};
    float stds[N_RAW_FEATURES] = {0};

    // Compute means
    for (int i = 0; i < WINDOW_SIZE; i++) {
        for (int j = 0; j < N_RAW_FEATURES; j++) {
            means[j] += buffer[i][j];
        }
    }
    for (int j = 0; j < N_RAW_FEATURES; j++) {
        means[j] /= WINDOW_SIZE;
    }
    ir_sum = means[6];  // IR mean

    // Compute std for accel/gyro (skip IR)
    for (int i = 0; i < WINDOW_SIZE; i++) {
        for (int j = 0; j < N_RAW_FEATURES - 1; j++) {  // Exclude IR
            float diff = buffer[i][j] - means[j];
            stds[j] += diff * diff;
        }
    }
    for (int j = 0; j < N_RAW_FEATURES - 1; j++) {
        stds[j] = sqrt(stds[j] / WINDOW_SIZE);
    }

    // Fill features: IR_mean, then stds for accel/gyro
    features_out[0] = ir_sum;
    for (int j = 0; j < N_RAW_FEATURES - 1; j++) {
        features_out[j + 1] = stds[j];
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mpu.begin()) {
        Serial.println("MPU6050 not found!");
        while (1) { delay(10); }
    }
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU6050 initialized");

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 not found!");
        while (1) { delay(10); }
    }
    particleSensor.setup(0x1F, 1, 2, 400, 411, 4096);
    particleSensor.clearFIFO();
    Serial.println("MAX30102 initialized");

    // Set RGB pins as output
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
}

void loop() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    long irValue = particleSensor.getIR();

    buffer[buffer_index][0] = a.acceleration.x;
    buffer[buffer_index][1] = a.acceleration.y;
    buffer[buffer_index][2] = a.acceleration.z;
    buffer[buffer_index][3] = g.gyro.x;
    buffer[buffer_index][4] = g.gyro.y;
    buffer[buffer_index][5] = g.gyro.z;
    buffer[buffer_index][6] = (float)irValue;
    buffer_index = (buffer_index + 1) % WINDOW_SIZE;

    static int sample_count = 0;
    sample_count++;
    if (sample_count >= WINDOW_SIZE) {
        float features[n_features];
        extract_features(features);
        int result = predict(features);
        Serial.print("Prediction: ");
        Serial.println(result ? "Awake" : "Sleep");
        sample_count = 0;
        if (result) {
          setColor(0, 255, 0, 100);
        }
        else{
          setColor(0, 0, 255, 100);
        }
    }

    delay(20);  // ~10 Hz
}

// Function to set color with brightness control
void setColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness) {
  analogWrite(RED_PIN, (red * brightness) / 255);
  analogWrite(GREEN_PIN, (green * brightness) / 255);
  analogWrite(BLUE_PIN, (blue * brightness) / 255);
}

/*
for wrong RGB:
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"
#include "svm_model.h"
#include "scaler_values.h"

#define RED_PIN   3
#define GREEN_PIN 4
#define BLUE_PIN  2

#define SDA_PIN 8
#define SCL_PIN 9
#define WINDOW_SIZE 50
#define N_RAW_FEATURES 7

Adafruit_MPU6050 mpu;
MAX30105 particleSensor;

float buffer[WINDOW_SIZE][N_RAW_FEATURES];
int buffer_index = 0;

// SVM prediction
int predict(float* features) {
    float norm_features[n_features];
    norm_features[0] = (features[0] - IR_mean_min) / (IR_mean_max - IR_mean_min);
    norm_features[1] = (features[1] - AccelX_std_min) / (AccelX_std_max - AccelX_std_min);
    norm_features[2] = (features[2] - AccelY_std_min) / (AccelY_std_max - AccelY_std_min);
    norm_features[3] = (features[3] - AccelZ_std_min) / (AccelZ_std_max - AccelZ_std_min);
    norm_features[4] = (features[4] - GyroX_std_min) / (GyroX_std_max - GyroX_std_min);
    norm_features[5] = (features[5] - GyroY_std_min) / (GyroY_std_max - GyroY_std_min);
    norm_features[6] = (features[6] - GyroZ_std_min) / (GyroZ_std_max - GyroZ_std_min);

    float decision = 0.0;
    for (int i = 0; i < n_support_vectors; i++) {
        float dot_product = 0.0;
        for (int j = 0; j < n_features; j++) {
            dot_product += norm_features[j] * support_vectors[i * n_features + j];
        }
        decision += dual_coef[i] * dot_product;
    }
    decision += intercept;
    return decision > 0 ? 1 : 0;
}

// Feature extraction
void extract_features(float* features_out) {
    float ir_sum = 0;
    float means[N_RAW_FEATURES] = {0};
    float stds[N_RAW_FEATURES] = {0};

    // Compute means
    for (int i = 0; i < WINDOW_SIZE; i++) {
        for (int j = 0; j < N_RAW_FEATURES; j++) {
            means[j] += buffer[i][j];
        }
    }
    for (int j = 0; j < N_RAW_FEATURES; j++) {
        means[j] /= WINDOW_SIZE;
    }
    ir_sum = means[6];  // IR mean

    // Compute std for accel/gyro (skip IR)
    for (int i = 0; i < WINDOW_SIZE; i++) {
        for (int j = 0; j < N_RAW_FEATURES - 1; j++) {  // Exclude IR
            float diff = buffer[i][j] - means[j];
            stds[j] += diff * diff;
        }
    }
    for (int j = 0; j < N_RAW_FEATURES - 1; j++) {
        stds[j] = sqrt(stds[j] / WINDOW_SIZE);
    }

    // Fill features: IR_mean, then stds for accel/gyro
    features_out[0] = ir_sum;
    for (int j = 0; j < N_RAW_FEATURES - 1; j++) {
        features_out[j + 1] = stds[j];
    }
}

void setup() {
    // Serial.begin(115200);
    // while (!Serial) { delay(10); }

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!mpu.begin()) {
        // Serial.println("MPU6050 not found!");
        while (1) { delay(10); }
    }
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    // Serial.println("MPU6050 initialized");

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        // Serial.println("MAX30102 not found!");
        while (1) { delay(10); }
    }
    particleSensor.setup(0x1F, 1, 2, 400, 411, 4096);
    particleSensor.clearFIFO();
    // Serial.println("MAX30102 initialized");

    // Set RGB pins as output
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
}

void loop() {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    long irValue = particleSensor.getIR();

    buffer[buffer_index][0] = a.acceleration.x;
    buffer[buffer_index][1] = a.acceleration.y;
    buffer[buffer_index][2] = a.acceleration.z;
    buffer[buffer_index][3] = g.gyro.x;
    buffer[buffer_index][4] = g.gyro.y;
    buffer[buffer_index][5] = g.gyro.z;
    buffer[buffer_index][6] = (float)irValue;
    buffer_index = (buffer_index + 1) % WINDOW_SIZE;

    static int sample_count = 0;
    sample_count++;
    if (sample_count >= WINDOW_SIZE) {
        float features[n_features];
        extract_features(features);
        int result = predict(features);
        // Serial.print("Prediction: ");
        // Serial.println(result ? "Awake" : "Sleep");
        sample_count = 0;
        if (result) {
          setColor(0, 255, 0, 10);
        }
        else{
          setColor(0, 0, 255, 10);
        }
    }

    delay(100);  // ~10 Hz
}

// Function to set color with brightness control
void setColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness) {
  analogWrite(RED_PIN, (red * brightness) / 255);
  analogWrite(GREEN_PIN, (green * brightness) / 255);
  analogWrite(BLUE_PIN, (blue * brightness) / 255);
}
*/