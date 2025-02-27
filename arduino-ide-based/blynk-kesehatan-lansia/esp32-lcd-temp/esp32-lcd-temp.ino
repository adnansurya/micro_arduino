#define BLYNK_TEMPLATE_ID "TMPL6-B_pH2Sg"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "SdgAnvD4LHTiYM6iePQAE_d-qTORZvlU"

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define RX_PIN 16  // Pin RX ESP32 (hubungkan ke TX Arduino Nano)
#define TX_PIN 17  // Pin TX ESP32 (tidak digunakan)

#define OUT_PIN 2

#define BLYNK_PRINT Serial

// Inisialisasi Sensor dan LCD
Adafruit_MLX90614 mlx = Adafruit_MLX90614();  // Sensor Suhu MLX90614
LiquidCrystal_I2C lcd(0x27, 16, 2);           // LCD I2C

BlynkTimer mainTimer;


// Blynk Auth Token
char ssid[] = "MIKRO";     // Ganti dengan nama WiFi Anda
char pass[] = "1DEAlist";  // Ganti dengan password WiFi Anda

// Variabel Data
float heartRate = 0;  // Detak jantung (bpm)
float spo2 = 0;       // Saturasi oksigen (%)
float accelX = 0;     // Akselerasi X (m/s²)
float accelY = 0;     // Akselerasi Y (m/s²)
float accelZ = 0;     // Akselerasi Z (m/s²)
float suhuTubuh = 0;  // Suhu tubuh (°C)

// Serial Input
String serialData = "";  // String untuk menyimpan data serial

String kondisi, lastKondisi;

float magnitude_g ;

void setup() {

  pinMode(OUT_PIN, OUTPUT);
  blinkOut(1);
  // Inisialisasi Serial
  Serial.begin(115200);                             // Serial Monitor untuk debugging
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);  // Serial untuk komunikasi dengan Arduino Nano

  // Inisialisasi Sensor Suhu (MLX90614)
  if (!mlx.begin()) {
    Serial.println("Gagal menginisialisasi MLX90614!");
    while (1)
      ;
  }

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Inisialisasi...");
  delay(2000);
  lcd.clear();

  // Inisialisasi Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Semua sensor berhasil diinisialisasi.\n");
  blinkOut(2);

  mainTimer.setInterval(1000L, mainEvent);
  mainTimer.setInterval(10000L, sendData);
}

void loop() {
  // Jalankan Blynk
  Blynk.run();
  mainTimer.run();
  // MPU6050: Magnitudo Akselerometer
  float magnitude = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);  // Magnitudo dalam m/s²
  magnitude_g = magnitude / 9.80665;                                      // Konversi ke satuan g (1g = 9.80665 m/s²)

  // Periksa apakah gerakan mendadak terdeteksi
  if (magnitude_g > 2.5) {  // Threshold gerakan mendadak dalam g
    kondisi += "Gerakan mendadak (kemungkinan jatuh)\n";
  }
}

// Fungsi untuk Memproses Data Serial
void parseSerialData(String data) {
  // Data format: 36.23/94.00/-0.08/0.69/10.02
  int index = 0;
  float values[5];  // Array untuk menyimpan data yang dipisahkan

  while (data.length() > 0) {
    int separatorIndex = data.indexOf('/');
    if (separatorIndex == -1) {  // Jika tidak ada separator lagi
      values[index] = data.toFloat();
      break;
    } else {
      values[index] = data.substring(0, separatorIndex).toFloat();
      data = data.substring(separatorIndex + 1);
      index++;
    }
  }

  // Simpan nilai ke variabel
  heartRate = values[0];
  spo2 = values[1];
  accelX = values[2];
  accelY = values[3];
  accelZ = values[4];

  // Debug Output
  Serial.print("HR: ");
  Serial.print(heartRate);
  Serial.print(" bpm, SpO2: ");
  Serial.print(spo2);
  Serial.print(" %, AccelX: ");
  Serial.print(accelX);
  Serial.print(" m/s², AccelY: ");
  Serial.print(accelY);
  Serial.print(" m/s², AccelZ: ");
  Serial.print(accelZ);
  Serial.println(" m/s²");
}

void checkHealthConditions() {
  kondisi = "";  // Reset string kondisi

  // MAX30100: Denyut Jantung (HR)
  if (heartRate < 60) {
    kondisi += "Bradikardia (<60 bpm)\n";
  } else if (heartRate > 100) {
    kondisi += "Takikardia (>100 bpm)\n";
  }

  // MAX30100: Saturasi Oksigen (SpO2)
  if (spo2 < 90) {
    kondisi += "Hipoksemia (<90%)\n";
  }

  // MLX90614: Suhu Tubuh
  if (suhuTubuh < 35.0) {
    kondisi += "Hipotermia (<35.0°C)\n";
  } else if (suhuTubuh > 37.5) {
    kondisi += "Demam ringan (>37.5°C)\n";
  }

  

  // Jika semua kondisi normal
  if (kondisi == "") {
    kondisi = "Semua kondisi normal";
  } else {
    blinkOut(1);
  }

  if (kondisi != lastKondisi) {
    // Kirim teks kondisi ke Blynk
    Blynk.virtualWrite(V0, kondisi);
    if (kondisi == "") {
      Blynk.logEvent("kondisi", "Semua Kondisi Normal");
    } else {
      Blynk.logEvent("kondisi", kondisi);
    }
  }

  // Debugging (opsional)
  Serial.print("Kondisi: ");
  Serial.println(kondisi);
  Serial.print("Magnitudo Akselerometer: ");
  Serial.print(magnitude_g);
  Serial.println(" g");

  lastKondisi = kondisi;
}

void mainEvent() {
  // Membaca data dari MLX90614 (Suhu Tubuh)
  suhuTubuh = mlx.readObjectTempC();
  if (isnan(suhuTubuh)) {
    suhuTubuh = 0;  // Jika pembacaan gagal, atur ke 0
  }

  // Membaca data dari Serial (Arduino Nano)
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {  // Jika data lengkap diterima (berakhir dengan newline '\n')
      parseSerialData(serialData);
      serialData = "";  // Reset string untuk data berikutnya
    } else {
      serialData += c;
    }
  }

  // Tampilkan data di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(suhuTubuh, 1);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("HR:");
  lcd.print(heartRate, 0);
  lcd.print("bpm O2:");
  lcd.print(spo2, 0);
  lcd.print("%");

  checkHealthConditions();
}


void blinkOut(int rep) {
  for (int i = 0; i < rep; i++) {
    digitalWrite(OUT_PIN, HIGH);
    delay(200);
    digitalWrite(OUT_PIN, LOW);
    delay(200);
  }
}

void sendData() {

  Serial.println("MENGIRIM DATA...");
  // Kirim data ke Blynk
  Blynk.virtualWrite(V1, suhuTubuh);  // Virtual Pin V1: Suhu tubuh
  Blynk.virtualWrite(V2, heartRate);  // Virtual Pin V2: Detak jantung
  Blynk.virtualWrite(V3, spo2);       // Virtual Pin V3: Saturasi oksigen
  
  // Kirim nilai magnitudo akselerometer ke Virtual Pin
  Blynk.virtualWrite(V4, magnitude_g);  // Virtual Pin V6 untuk Magnitudo (g)
}
