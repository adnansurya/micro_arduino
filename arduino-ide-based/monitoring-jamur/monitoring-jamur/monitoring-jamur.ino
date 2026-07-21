#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"

// ==========================================================
// PENYESUAIAN PIN ESP32-S2 (LOLIN S2 MINI)
// ==========================================================
#define DHTPIN 39         // Sensor DHT22 di GPIO 39
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define BUZZER_PIN 40     // Buzzer di GPIO 40

#define SD_CS_PIN 12       // Pin Chip Select (CS) Micro SD Card Reader
// ==========================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
// OLED menggunakan pin I2C default S2 Mini (SDA: 33, SCL: 34)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variabel Konfigurasi Range & Delta dari Google Sheets (dengan nilai default)
float suhuMin = 22.0;
float suhuMax = 28.0;
float kelembabanMin = 70.0;
float kelembabanMax = 90.0;
float deltaSuhu = 0.5;
float deltaKelembaban = 2.0;
long intervalData = 60000;

// Variabel data sensor
float suhuSesaat = 0.0;
float kelembabanSesaat = 0.0;
float suhuTerkirim = 0.0;
float kelembabanTerkirim = 0.0;

// Variabel Penanda Waktu (millis)
unsigned long waktuTerakhirOLED = 0;
unsigned long waktuTerakhirKirim = 0;
unsigned long waktuTerakhirBuzzer = 0;
unsigned long waktuResetIkon = 0;

// Variabel Status Indikator
int statusKirimIcon = 0;
bool isOutOfRange = false;  

void fetchThresholds() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Fetching Config...");
    display.display();

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String urlWithParam = String(WEB_APP_URL) + "?action=get_config";
    http.begin(urlWithParam);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        suhuMin = doc["suhu_min"];
        suhuMax = doc["suhu_max"];
        kelembabanMin = doc["kelembaban_min"];
        kelembabanMax = doc["kelembaban_max"];
        deltaSuhu = doc["delta_suhu"];
        deltaKelembaban = doc["delta_kelembaban"];
        intervalData = doc["interval_data"].as<long>() * 1000;

        Serial.println("--- CONFIG RENTANG BERHASIL DI-LOAD ---");
      }
    }
    http.end();
    delay(2000);
  }
}

// Fungsi untuk Menyimpan Data secara Lokal ke local.csv
void simpanKeSDCard(float t, float h) {
  File dataFile = SD.open("/local.csv", FILE_APPEND);
  
  if (dataFile) {
    // Jika file baru/kosong, tulis header CSV terlebih dahulu
    if (dataFile.size() == 0) {
      dataFile.println("Timestamp_Millis,Suhu,Kelembaban");
    }
    
    // Simpan millis saat ini sebagai penanda waktu relatif (atau bisa diganti NTP jika ada)
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.print(t, 1);
    dataFile.print(",");
    dataFile.println(h, 1);
    dataFile.close();
    
    Serial.println("Data berhasil disimpan ke local.csv");
  } else {
    Serial.println("Gagal membuka local.csv untuk penulisan!");
  }
}

// Fungsi menggambar ikon pengiriman data (Ujung Kanan Bawah)
void drawStatusIcon(int status) {
  display.setTextSize(1);
  int iconX = 118;
  int iconY = 52;  

  if (status == 1) {
    display.setCursor(iconX, iconY);
    display.print("^");
  } else if (status == 2) {
    display.drawLine(iconX, iconY + 4, iconX + 3, iconY + 7, SSD1306_WHITE);
    display.drawLine(iconX + 3, iconY + 7, iconX + 8, iconY + 1, SSD1306_WHITE);
  } else if (status == 3) {
    display.setCursor(iconX, iconY);
    display.print("X");
  }
}

// Fungsi menggambar simbol Peringatan 'o'
void drawWarningIcon(bool outOfRange) {
  if (outOfRange && digitalRead(BUZZER_PIN) == HIGH) {
    int warnX = 118;
    int warnY = 38;
    display.setCursor(warnX, warnY);
    display.setTextSize(1);
    display.print("o");
  }
}

void kirimDataKeGoogle(float t, float h) {
  // 1. Simpan terlebih dahulu ke Micro SD sebelum dikirim
  simpanKeSDCard(t, h);

  // 2. Kirim data ke Cloud (Google Sheets)
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(WEB_APP_URL);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["suhu"] = t;
    doc["kelembaban"] = h;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    statusKirimIcon = 1;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("MONITORING SENSOR");
    display.println("---------------------");

    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("T:");       
    display.setCursor(30, 20);  
    display.print(suhuSesaat, 1);

    display.setTextSize(1);
    display.print(" o");
    display.setTextSize(2);
    display.print("C");

    display.setCursor(0, 42);
    display.print("H:");       
    display.setCursor(30, 42);  
    display.print(kelembabanSesaat, 1);
    display.print(" %");

    drawStatusIcon(statusKirimIcon);
    drawWarningIcon(isOutOfRange);

    display.display();

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      statusKirimIcon = 2;
      suhuTerkirim = t;
      kelembabanTerkirim = h;
    } else {
      statusKirimIcon = 3;
    }
    http.end();
  } else {
    statusKirimIcon = 3;
  }
  waktuResetIkon = millis();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Inisialisasi I2C OLED (SDA: 33, SCL: 35)
  Wire.begin(33, 35);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;)
      ;
  }

  // Inisialisasi SPI & Micro SD Card
  SPI.begin(7, 9, 11, SD_CS_PIN); // SCK=7, MISO=9, MOSI=11, CS=12
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Inisialisasi Micro SD Gagal / Kartu tidak terdeteksi!");
  } else {
    Serial.println("Micro SD Berhasil Diinisialisasi.");
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  fetchThresholds();

  suhuSesaat = dht.readTemperature();
  kelembabanSesaat = dht.readHumidity();

  if (!isnan(suhuSesaat) && !isnan(kelembabanSesaat)) {
    kirimDataKeGoogle(suhuSesaat, kelembabanSesaat);
  }

  waktuTerakhirKirim = millis();
}

void loop() {
  unsigned long waktuSekarang = millis();

  if (waktuSekarang - waktuTerakhirOLED >= 200) {  
    waktuTerakhirOLED = waktuSekarang;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      suhuSesaat = t;
      kelembabanSesaat = h;
    }

    if (suhuSesaat < suhuMin || suhuSesaat > suhuMax || kelembabanSesaat < kelembabanMin || kelembabanSesaat > kelembabanMax) {
      isOutOfRange = true;
    } else {
      isOutOfRange = false;
    }

    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("MONITORING SENSOR");
    display.println("---------------------");

    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("T:");       
    display.setCursor(30, 20);  
    display.print(suhuSesaat, 1);

    display.setTextSize(1);
    display.print(" o");
    display.setTextSize(2);
    display.print("C");

    display.setCursor(0, 42);
    display.print("H:");       
    display.setCursor(30, 42);  
    display.print(kelembabanSesaat, 1);
    display.print(" %");

    drawStatusIcon(statusKirimIcon);
    drawWarningIcon(isOutOfRange);

    display.display();
  }

  if (statusKirimIcon > 1 && (waktuSekarang - waktuResetIkon >= 5000)) {
    statusKirimIcon = 0;
  }

  float selisihSuhu = abs(suhuSesaat - suhuTerkirim);
  float selisihKelembaban = abs(kelembabanSesaat - kelembabanTerkirim);

  bool deltaTerlampaui = (selisihSuhu >= deltaSuhu) || (selisihKelembaban >= deltaKelembaban);
  bool intervalTerlampaui = (waktuSekarang - waktuTerakhirKirim >= intervalData);

  if (deltaTerlampaui || intervalTerlampaui) {
    waktuTerakhirKirim = waktuSekarang;
    kirimDataKeGoogle(suhuSesaat, kelembabanSesaat);
  }

  if (isOutOfRange) {
    if (waktuSekarang - waktuTerakhirBuzzer >= 200) {
      waktuTerakhirBuzzer = waktuSekarang;
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}