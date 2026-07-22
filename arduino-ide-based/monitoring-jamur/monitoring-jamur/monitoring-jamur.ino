#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h> // Tambahan Library OTA
#include "config.h"

// ==========================================================
// PENYESUAIAN PIN ESP32 DEVKIT V1 (30 PIN)
// ==========================================================
#define DHTPIN 15          // Sensor DHT22 di GPIO 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define BUZZER_PIN 25     // Buzzer di GPIO 25
#define SD_CS_PIN 5       // Pin Chip Select (CS) Micro SD Card Reader (VSPI CS)
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
    return String(millis()); 
  }
  char buffer[30];
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

    WiFiClientSecure client;
    client.setInsecure();

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String urlWithParam = String(WEB_APP_URL) + "?action=get_config";
    
    if (http.begin(client, urlWithParam)) {
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
    }
    delay(1000);
  }
}

// Fungsi Simpan ke local.csv
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

// Fungsi Simpan ke backup.csv
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

  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  bool headerSkipped = false;
  while (dataFile.available()) {
    String line = dataFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (!headerSkipped && line.startsWith("Timestamp")) {
      headerSkipped = true;
      continue;
    }

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

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (http.begin(client, String(WEB_APP_URL) + "?action=sync_batch")) {
      http.addHeader("Content-Type", "application/json");

      int httpCode = http.POST(jsonPayload);
      if (httpCode > 0) {
        Serial.println("Sinkronisasi backup berhasil!");
        SD.remove("/backup.csv"); 
      } else {
        Serial.println("Gagal sinkronisasi backup ke server.");
      }
      http.end();
    }
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

  Serial.println("\n----------------------------------------");
  Serial.println("[PROSES] Memulai pengiriman data sensor...");
  Serial.print("[INFO] Timestamp: "); Serial.println(timestampStr);
  Serial.print("[INFO] Suhu: "); Serial.print(t, 1); Serial.println(" C");
  Serial.print("[INFO] Kelembaban: "); Serial.print(h, 1); Serial.println(" %");

  simpanKeSDCard(timestampStr, t, h);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] Status: Terhubung ke jaringan.");
    
    WiFiClientSecure client;
    client.setInsecure(); 

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial.print("[HTTP] Menghubungkan ke URL: "); Serial.println(WEB_APP_URL);
    
    if (http.begin(client, WEB_APP_URL)) {
      http.addHeader("Content-Type", "application/json");

      JsonDocument doc;
      doc["timestamp"] = timestampStr; 
      doc["suhu"] = t;
      doc["kelembaban"] = h;

      String jsonPayload;
      serializeJson(doc, jsonPayload);
      
      Serial.print("[JSON] Payload yang dikirim: "); 
      Serial.println(jsonPayload);

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

      Serial.println("[HTTP] Mengirim HTTP POST request...");
      int httpResponseCode = http.POST(jsonPayload);

      if (httpResponseCode > 0) {
        Serial.print("[HTTP] Berhasil! Response Code: ");
        Serial.println(httpResponseCode);
        
        String responsePayload = http.getString();
        Serial.print("[HTTP] Response Body: ");
        Serial.println(responsePayload);

        statusKirimIcon = 2;
        suhuTerkirim = t;
        kelembabanTerkirim = h;
      } else {
        Serial.print("[ERROR] HTTP POST Gagal, Error Code: ");
        Serial.println(httpResponseCode);
        Serial.print("[ERROR] Keterangan: ");
        Serial.println(http.errorToString(httpResponseCode).c_str());

        statusKirimIcon = 3;
        simpanKeBackupCard(timestampStr, t, h); 
      }
      http.end();
    } else {
      Serial.println("[ERROR] Gagal menginisialisasi koneksi HTTPClient.");
      statusKirimIcon = 3;
      simpanKeBackupCard(timestampStr, t, h);
    }
  } else {
    Serial.println("[ERROR] WiFi Terputus! Tidak dapat mengirim data ke cloud.");
    statusKirimIcon = 3;
    simpanKeBackupCard(timestampStr, t, h); 
  }
  
  Serial.println("[PROSES] Pengiriman data selesai.");
  Serial.println("----------------------------------------\n");
  waktuResetIkon = millis();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Inisialisasi I2C OLED SSD1306 (SDA = GPIO 21, SCL = GPIO 22)
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  // Inisialisasi SPI Micro SD (SCK = 18, MISO = 19, MOSI = 23, CS = 5)
  SPI.begin(18, 19, 23, SD_CS_PIN); 
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

  // Inisialisasi WiFi Manager
  WiFiManager wifiManager;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Setup Mode");
  display.println("Connect to AP:");
  display.println("ESP32-Sensor-Config");
  display.display();

  bool res = wifiManager.autoConnect("ESP32-Sensor-Config");

  if (!res) {
    Serial.println("Gagal terhubung / Waktu habis konfigurasi");
  } else {
    Serial.println("Berhasil terhubung ke WiFi!");
  }

  // ==========================================================
  // KONFIGURASI ARDUINO OTA
  // ==========================================================
  ArduinoOTA.setHostname("ESP32-Jamur-Sensor"); // Nama hostname perangkat di jaringan lokal

  // Password untuk keamanan saat upload OTA
  ArduinoOTA.setPassword("admin123");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd Update OTA Selesai");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress OTA: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  // ==========================================================

  // Sinkronisasi Waktu NTP
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Load Config & Data Backup
  fetchThresholds();
  sinkronisasiBackupData();

  suhuSesaat = dht.readTemperature();
  kelembabanSesaat = dht.readHumidity();

  if (!isnan(suhuSesaat) && !isnan(kelembabanSesaat)) {
    kirimDataKeGoogle(suhuSesaat, kelembabanSesaat);
  }

  waktuTerakhirKirim = millis();
}

void loop() {
  // PENTING: Handle proses OTA di setiap perulangan loop
  ArduinoOTA.handle();

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