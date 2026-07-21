#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include "config.h"

// ==========================================================
// PENYESUAIAN PIN ESP32-S2 (LOLIN S2 MINI)
// ==========================================================
#define DHTPIN 39         // Sensor DHT22 di GPIO 39
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define BUZZER_PIN 40     // Buzzer di GPIO 40
#define SD_CS_PIN 12      // Pin Chip Select (CS) Micro SD Card Reader
// ==========================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variabel Konfigurasi Range & Delta dari Google Sheets
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

// Fungsi Mendapatkan Waktu Format: M/D/YYYY H:M:S via NTP
String getFormattedTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String(millis()); // Fallback ke millis jika NTP belum sinkron
  }
  char buffer[30];
  // Format: M/D/YYYY H:M:S (Contoh: 7/21/2026 17:35:18)
  strftime(buffer, sizeof(buffer), "%m/%d/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

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
    delay(1000);
  }
}

// Fungsi Simpan ke local.csv (Real-time normal)
void simpanKeSDCard(String timestamp, float t, float h) {
  File dataFile = SD.open("/local.csv", FILE_APPEND);
  if (dataFile) {
    if (dataFile.size() == 0) {
      dataFile.println("Timestamp,Suhu,Kelembaban");
    }
    dataFile.print(timestamp);
    dataFile.print(",");
    dataFile.print(t, 1);
    dataFile.print(",");
    dataFile.println(h, 1);
    dataFile.close();
  }
}

// Fungsi Simpan ke backup.csv (Saat internet/kirim putus)
void simpanKeBackupCard(String timestamp, float t, float h) {
  File dataFile = SD.open("/backup.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.print(timestamp);
    dataFile.print(",");
    dataFile.print(t, 1);
    dataFile.print(",");
    dataFile.println(h, 1);
    dataFile.close();
    Serial.println("Data gagal terkirim, disimpan ke backup.csv");
  }
}

// Fungsi Sinkronisasi Data Backup saat Startup
void sinkronisasiBackupData() {
  if (!SD.exists("/backup.csv")) {
    Serial.println("Tidak ada file backup.csv. Lanjut normal.");
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Syncing Backup Data...");
  display.display();

  File dataFile = SD.open("/backup.csv", FILE_READ);
  if (!dataFile) return;

  // Baca baris per baris file CSV dan bungkus ke JSON Array
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  bool headerSkipped = false;
  while (dataFile.available()) {
    String line = dataFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Lewati baris header jika ada
    if (!headerSkipped && line.startsWith("Timestamp")) {
      headerSkipped = true;
      continue;
    }

    // Parsing baris CSV (Timestamp,Suhu,Kelembaban)
    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    if (comma1 != -1 && comma2 != -1) {
      String ts = line.substring(0, comma1);
      float t = line.substring(comma1 + 1, comma2).toFloat();
      float h = line.substring(comma2 + 1).toFloat();

      JsonObject obj = array.add<JsonObject>();
      obj["timestamp"] = ts;
      obj["suhu"] = t;
      obj["kelembaban"] = h;
    }
  }
  dataFile.close();

  if (array.size() > 0 && WiFi.status() == WL_CONNECTED) {
    String jsonPayload;
    serializeJson(doc, jsonPayload);

    HTTPClient http;
    // Kirim dengan parameter action=sync_batch
    http.begin(String(WEB_APP_URL) + "?action=sync_batch");
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);
    if (httpCode > 0) {
      Serial.println("Sinkronisasi backup berhasil!");
      SD.remove("/backup.csv"); // Hapus file jika sukses dikirim
    } else {
      Serial.println("Gagal sinkronisasi backup ke server.");
    }
    http.end();
  }
}

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
  String timestampStr = getFormattedTimestamp();

  // 1. Simpan rutin ke local.csv
  simpanKeSDCard(timestampStr, t, h);

  // 2. Kirim data ke Cloud (Google Sheets)
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(WEB_APP_URL);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["timestamp"] = timestampStr; // Kirim timestamp NTP ke Apps Script
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
      simpanKeBackupCard(timestampStr, t, h); // Gagal kirim -> Simpan ke backup
    }
    http.end();
  } else {
    statusKirimIcon = 3;
    simpanKeBackupCard(timestampStr, t, h); // Tidak ada WiFi -> Simpan ke backup
  }
  waktuResetIkon = millis();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(33, 35);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  SPI.begin(7, 9, 11, SD_CS_PIN); 
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Inisialisasi Micro SD Gagal!");
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

  // Sinkronisasi Waktu NTP Indonesia (WITA / UTC+8 atau sesuaikan zona waktu)
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Load Config Batas Aman dari Google Sheets
  fetchThresholds();

  // Cek & Sinkronkan Backup Data dari SD Card sebelum masuk loop
  sinkronisasiBackupData();

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