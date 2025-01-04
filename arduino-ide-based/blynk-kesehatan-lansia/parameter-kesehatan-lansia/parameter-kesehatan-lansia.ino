#define BLYNK_TEMPLATE_ID "TMPL6-B_pH2Sg"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "SdgAnvD4LHTiYM6iePQAE_d-qTORZvlU"

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <MAX30100_PulseOximeter.h>  // Library Oxullo untuk MAX30100
#include <Adafruit_MPU6050.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

// Inisialisasi sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
PulseOximeter pox;     // Inisialisasi MAX30100 dari library Oxullo
Adafruit_MPU6050 mpu;  // Inisialisasi MPU6050 dari library Adafruit

// Variabel untuk data sensor
double suhuTubuh;
float heartRate, spO2;
float accelX, accelY, accelZ;  // Data akselerasi
float gyroX, gyroY, gyroZ;     // Data gyroscope

// Variabel untuk teks kondisi
String kondisi = "";

// Blynk Auth Token
char ssid[] = "MIKRO";     // Ganti dengan nama WiFi Anda
char pass[] = "1DEAlist";  // Ganti dengan password WiFi Anda

BlynkTimer timer;

// Callback untuk pembaruan data dari MAX30100
void onPulseOximeterData() {
  Serial.println("Beat!");
}

void updateData() {
  // Baca data suhu tubuh dari MLX90614
  suhuTubuh = mlx.readObjectTempC();
  Serial.println("Suhu Tubuh: " + String(suhuTubuh) + " °C");

  // Update data dari MAX30100
  pox.update();

  heartRate = pox.getHeartRate();
  spO2 = pox.getSpO2();

  Serial.println("Denyut Jantung: " + String(heartRate) + " bpm");
  Serial.println("Saturasi Oksigen: " + String(spO2) + " %");

  // Baca data akselerasi dan gyroscope dari MPU6050
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  accelX = accel.acceleration.x;
  accelY = accel.acceleration.y;
  accelZ = accel.acceleration.z;
  gyroX = gyro.gyro.x;
  gyroY = gyro.gyro.y;
  gyroZ = gyro.gyro.z;



  Serial.println("Akselerasi: ax=" + String(accelX) + " m/s², ay=" + String(accelY) + " m/s², az=" + String(accelZ) + " m/s²");

  // Kirim data ke Blynk Virtual Pins dengan satuan
  Blynk.virtualWrite(V1, String(suhuTubuh) + " °C");                                                    // Virtual Pin V1 untuk Suhu Tubuh
  Blynk.virtualWrite(V2, String(heartRate) + " bpm");                                                   // Virtual Pin V2 untuk Denyut Jantung
  Blynk.virtualWrite(V3, String(spO2) + " %");                                                          // Virtual Pin V3 untuk Saturasi Oksigen
  Blynk.virtualWrite(V4, String(sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ)) + " m/s²");  // Virtual Pin V4 untuk Magnitudo Gerakan

  // Periksa dan kirim teks kondisi ke Blynk (V0)
  checkHealthConditions();
}

// Fungsi untuk memeriksa kondisi kesehatan
void checkHealthConditions() {
  kondisi = "";  // Reset string kondisi

  // MAX30100: Denyut Jantung (HR)
  if (heartRate < 60) {
    kondisi += "Bradikardia (<60 bpm); ";
  } else if (heartRate > 100) {
    kondisi += "Takikardia (>100 bpm); ";
  }

  // MAX30100: Saturasi Oksigen (SpO2)
  if (spO2 < 90) {
    kondisi += "Hipoksemia (<90%); ";
  }

  // MLX90614: Suhu Tubuh
  if (suhuTubuh < 36.0) {
    kondisi += "Hipotermia (<36.0°C); ";
  } else if (suhuTubuh > 37.5) {
    kondisi += "Demam ringan (>37.5°C); ";
  }

  // MPU6050: Gerakan
  float magnitude = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);  // Magnitudo akselerasi
  if (magnitude > 2.5) {                                                        // Threshold untuk mendeteksi gerakan mendadak
    kondisi += "Gerakan mendadak (kemungkinan jatuh); ";
  }

  // Jika semua kondisi normal
  if (kondisi == "") {
    kondisi = "Semua kondisi normal";
  }

  // Kirim teks kondisi ke Blynk
  Blynk.virtualWrite(V0, kondisi);
}

void setup() {
  Serial.begin(115200);


  // Inisialisasi WiFi dan Blynk

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);


  // Inisialisasi sensor suhu (MLX90614)
  if (!mlx.begin()) {
    Serial.println("MLX90614 gagal diinisialisasi!");
    while (1)
      ;
  }

  // Inisialisasi sensor MAX30100
  if (!pox.begin()) {
    Serial.println("MAX30100 gagal diinisialisasi!");
    while (1)
      ;
  }
  pox.setOnBeatDetectedCallback(onPulseOximeterData);

  // Inisialisasi sensor MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 gagal diinisialisasi!");
    while (1)
      ;
  }

  timer.setInterval(1000L, updateData);
}

void loop() {
  // Jalankan Blynk
  Blynk.run();
  timer.run();
}
