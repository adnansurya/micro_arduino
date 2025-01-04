#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_MAX30100.h>
#include <MPU6050.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// Inisialisasi sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_MAX30100 max30100;
MPU6050 mpu;

// Variabel untuk data sensor
float suhuTubuh;
float heartRate, spO2;
int16_t ax, ay, az, gx, gy, gz;

// Variabel untuk teks kondisi
String kondisi = "";

// Blynk Auth Token
char auth[] = "Your_Blynk_Auth_Token";  // Ganti dengan Auth Token Anda
char ssid[] = "Your_WiFi_SSID";         // Ganti dengan nama WiFi Anda
char pass[] = "Your_WiFi_Password";     // Ganti dengan password WiFi Anda

// Fungsi untuk memeriksa kondisi kesehatan
void checkHealthConditions() {
  kondisi = ""; // Reset string kondisi

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
  float magnitude = sqrt(ax * ax + ay * ay + az * az) / 16384.0; // Normalisasi akselerasi
  if (magnitude > 2.5) { // Threshold untuk mendeteksi gerakan mendadak
    kondisi += "Gerakan mendadak (kemungkinan jatuh); ";
  }

  // Jika semua kondisi normal
  if (kondisi == "") {
    kondisi = "Semua kondisi normal";
  }

  // Kirim teks kondisi ke Blynk
  Blynk.virtualWrite(V5, kondisi);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Inisialisasi WiFi dan Blynk
  WiFi.begin(ssid, pass);
  Blynk.begin(auth, ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");

  // Inisialisasi sensor
  if (!mlx.begin()) {
    Serial.println("MLX90614 gagal diinisialisasi!");
    while (1);
  }
  max30100.begin();
  max30100.setMode(MAX30100_MODE_SPO2_HR);
  max30100.setLedsPulseAmplitude(0x1F, 0x1F);
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 gagal diinisialisasi!");
    while (1);
  }
}

void loop() {
  // Jalankan Blynk
  Blynk.run();

  // Baca data suhu tubuh dari MLX90614
  suhuTubuh = mlx.readObjectTempC();
  Serial.println("Suhu Tubuh: " + String(suhuTubuh) + " °C");

  // Baca data denyut jantung dan SpO2 dari MAX30100
  max30100.update();
  heartRate = max30100.getHeartRate();
  spO2 = max30100.getSpO2();
  Serial.println("Denyut Jantung: " + String(heartRate) + " bpm");
  Serial.println("Saturasi Oksigen: " + String(spO2) + " %");

  // Baca data akselerometer dari MPU6050
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  Serial.println("Akselerasi: ax=" + String(ax) + ", ay=" + String(ay) + ", az=" + String(az));

  // Kirim data ke Blynk Virtual Pins
  Blynk.virtualWrite(V1, suhuTubuh);  // Virtual Pin V1 untuk Suhu Tubuh
  Blynk.virtualWrite(V2, heartRate); // Virtual Pin V2 untuk Denyut Jantung
  Blynk.virtualWrite(V3, spO2);      // Virtual Pin V3 untuk Saturasi Oksigen
  Blynk.virtualWrite(V4, sqrt(ax * ax + ay * ay + az * az)); // Virtual Pin V4 untuk Magnitudo Gerakan

  // Periksa dan kirim teks kondisi ke Blynk
  checkHealthConditions();

  // Tunggu 1 detik sebelum membaca ulang data
  delay(1000);
}
