#include <ModbusMaster.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>
#include <WiFiManager.h>
#include <time.h> // Library internal ESP32 untuk NTP

// MEMANGGIL FILE KONFIGURASI TERPISAH
#include "config.h"

// Konfigurasi Hardware Serial untuk RS485
HardwareSerial modbusSerial(2);  
ModbusMaster node;

// Konfigurasi RTC DS3231
RTC_DS3231 rtc;

// Konfigurasi SD Card
const int chipSelect = 5;  
File dataFile;

// Konfigurasi WiFi Manager
WiFiManager wm;

// =========================================================================
// KONFIGURASI NTP SERVER & OFFSET MANUAL
// =========================================================================
const char* ntpServer = "id.pool.ntp.org"; // Server NTP Indonesia (lebih cepat & stabil)
const long  gmtOffset_sec = 8 * 3600;      // UTC+8 (WITA) = 8 jam * 3600 detik = 28800
const int   daylightOffset_sec = 0;        // Tidak ada DST di Indonesia
// =========================================================================

// Alamat modbus device
static uint8_t modbusAddr = 1;

struct RegisterDefinition {
  uint16_t address;
  const char* name;
  const char* dataType;
  const char* jsonKey;
  float scaleFactor;
};

struct ReadingResult {
  const char* name;
  const char* jsonKey;
  float value;
};

RegisterDefinition registersToRead[] = {
  { 0x0042, "R phase Voltage", "U32", "voltageR", 10000.0 },
  { 0x0044, "S phase Voltage", "U32", "voltageS", 10000.0 },
  { 0x0046, "T phase Voltage", "U32", "voltageT", 10000.0 },
  { 0x0058, "R phase Current", "U32", "currentR", 10000.0 },
  { 0x005A, "S phase Current", "U32", "currentS", 10000.0 },
  { 0x005C, "T phase Current", "U32", "currentT", 10000.0 }
};

const int numRegisters = sizeof(registersToRead) / sizeof(registersToRead[0]);
ReadingResult readingResults[numRegisters];

bool sdCardAvailable = false;
bool rtcAvailable = false;
bool wifiConnected = false;

const String dataFileName = "/data_log.csv";
const String statusFileName = "/system_status.txt";

bool shouldSaveConfig = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; 

bool timeSynced = false;
unsigned long lastTimeSync = 0;
const unsigned long TIME_SYNC_INTERVAL = 24 * 60 * 60 * 1000; // Sync harian otomatis

// =========================================================================
// MANAJEMEN TIMER INTERVAL
unsigned long lastSDSaveTime = 0;
unsigned long lastAPISendTime = 0;

const unsigned long sdSaveInterval = 10000;             
unsigned long apiSendInterval = 5 * 60 * 1000;          
// =========================================================================

const String FIRMWARE_VERSION = "2.3.0-NTP-NATIVE";

void setup() {
  modbusSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(115200);

  Serial.println("\n==================================");
  Serial.println("   ESP32 Data Logger - MODBUS RTU");
  Serial.print("   Firmware Version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("==================================");

  if (rtc.begin()) {
    rtcAvailable = true;
    Serial.println("✅ RTC DS3231 terhubung");
  } else {
    Serial.println("❌ Gagal terhubung ke RTC DS3231");
  }

  if (SD.begin(chipSelect)) {
    sdCardAvailable = true;
    Serial.println("✅ SD Card terhubung");
    createCSVHeader();
    saveSystemStatus("BOOT", "System started in setup");
  } else {
    Serial.println("❌ Gagal terhubung ke SD Card");
  }

  setupWiFiManager();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🔌 OFFLINE MODE: WiFi tidak tersedia saat boot");
    saveSystemStatus("OFFLINE_MODE", "WiFi not available at boot");
  } else {
    wifiConnected = true;
    Serial.println("📡 ONLINE MODE: Terhubung ke WiFi");
    saveSystemStatus("ONLINE_MODE", "WiFi connected successfully");
    
    // Konfigurasi NTP bawaan ESP32 dengan offset manual
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // 4. PERCOBAAN SINKRONISASI WAKTU DARI SERVER NTP (Maks 1 menit)
    if (rtcAvailable) {
      Serial.println("🕒 Memulai sinkronisasi waktu dari NTP Server...");
      unsigned long startSyncAttempt = millis();
      bool syncSuccess = false;

      while (millis() - startSyncAttempt < 60000) {
        if (syncTimeFromNTP()) {
          syncSuccess = true;
          break;
        }
        Serial.print(".");
        delay(1000); 
      }

      if (syncSuccess) {
        Serial.println("\n✅ Waktu RTC berhasil disinkronkan via NTP (UTC+8)");
      } else {
        Serial.println("\n⚠️ Batas waktu 1 menit habis. Menggunakan waktu lama di RTC.");
      }
    }

    // 5. KIRIM PESAN SISTEM SIAP + WAKTU RTC SAAT INI
    String bootMessage = "Sistem Data Logger Aktif!\n";
    bootMessage += "Firmware: " + FIRMWARE_VERSION + "\n";
    if (rtcAvailable) {
      bootMessage += "Waktu RTC (UTC+8): " + getDateTimeString(rtc.now());
    } else {
      bootMessage += "Waktu RTC: Jam RTC Rusak/Tidak Terdeteksi";
    }
    
    sendMessage(bootMessage);
  }

  printSystemInfo();
}

void loop() {
  unsigned long currentMillis = millis();

  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    if (currentMillis - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = currentMillis;
      Serial.println("⚠️ Koneksi WiFi terputus, mencoba reconnect...");
      WiFi.reconnect();
    }
  } else {
    if (!wifiConnected) {
      wifiConnected = true;
      Serial.println("✅ WiFi Terhubung Kembali");
      saveSystemStatus("WIFI_RECONNECT", "WiFi reconnected");
      if (rtcAvailable && !timeSynced) {
        syncTimeFromNTP();
      }
    }
  }

  if (wifiConnected && rtcAvailable) {
    if (currentMillis - lastTimeSync > TIME_SYNC_INTERVAL) {
      syncTimeFromNTP();
    }
  }

  if (currentMillis - lastSDSaveTime >= sdSaveInterval) {
    lastSDSaveTime = currentMillis;
    baca_registers_statistik();
    simpan_data_ke_csv();
  }

  if (currentMillis - lastAPISendTime >= apiSendInterval) {
    lastAPISendTime = currentMillis;

    String formatPesan = "Laporan Data Sensor:\n";
    if (rtcAvailable) {
      formatPesan += "Waktu: " + getDateTimeString(rtc.now()) + "\n";
    }
    formatPesan += "VR: " + String(readingResults[0].value, 2) + " V\n";
    formatPesan += "VS: " + String(readingResults[1].value, 2) + " V\n";
    formatPesan += "VT: " + String(readingResults[2].value, 2) + " V\n";
    formatPesan += "IR: " + String(readingResults[3].value, 2) + " A\n";
    formatPesan += "IS: " + String(readingResults[4].value, 2) + " A\n";
    formatPesan += "IT: " + String(readingResults[5].value, 2) + " A";

    sendMessage(formatPesan);
  }
}

void sendMessage(String pesan) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<512> doc;
    doc["user_code"] = user_code;
    doc["secret"] = secret;
    doc["device_id"] = device_id;
    doc["phone"] = phone;
    doc["message"] = pesan;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.println("📤 Mengirim Pesan HTTP POST...");
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println("Response: " + http.getString());
    } else {
      Serial.print("Error: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected, Gagal mengirim pesan.");
  }
  Serial.println("\n--- Selesai ---");
}

// FUNGSI BARU UNTUK SYNC DENGAN NTP NATIVE & OFFSET MANUAL
bool syncTimeFromNTP() {
  if (!rtcAvailable) return false;
  
  struct tm timeinfo;
  // Mencoba mengambil waktu lokal yang sudah ditambahkan offset otomatis oleh core ESP32
  if (!getLocalTime(&timeinfo)) {
    return false; 
  }

  // Set hasil waktu NTP ke module RTC DS3231
  DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  rtc.adjust(ntpTime);
  
  timeSynced = true;
  lastTimeSync = millis();
  return true;
}

void baca_registers_statistik() {
  Serial.println("\n--- Membaca Register Modbus ---");
  for (int i = 0; i < numRegisters; i++) {
    float floatValue = getFloatData(registersToRead[i].address, registersToRead[i].dataType);
    float scaledValue = floatValue / registersToRead[i].scaleFactor;

    readingResults[i].name = registersToRead[i].name;
    readingResults[i].jsonKey = registersToRead[i].jsonKey;
    readingResults[i].value = isnan(scaledValue) ? 0.0 : scaledValue;

    Serial.printf("%s: %.4f\n", readingResults[i].name, readingResults[i].value);
  }
}

void simpan_data_ke_csv() {
  if (!sdCardAvailable) {
    Serial.println("❌ Gagal simpan CSV: SD Card tidak terdeteksi");
    return;
  }
  
  File file = SD.open(dataFileName, FILE_APPEND);
  if (!file) return;

  file.print(rtcAvailable ? getDateTimeString(rtc.now()) : String(millis()));
  file.print(",");

  for (int i = 0; i < numRegisters; i++) {
    file.print(readingResults[i].value, 4);
    if (i < numRegisters - 1) file.print(",");
  }
  file.println();
  file.close();
  Serial.println("💾 Data berhasil dicatat ke CSV (10 Detikan)");
}

void setupWiFiManager() {
  wm.setSaveConfigCallback([](){ shouldSaveConfig = true; });
  wm.setConfigPortalTimeout(120);
  wm.setConnectTimeout(30);
  wm.setHostname("esp32-datalogger");

  bool res = wm.autoConnect("ESP32-DataLogger", "password123");
  if (!res) {
    saveSystemStatus("WIFI_FAIL", "Failed to connect to WiFi at boot");
  } else {
    saveSystemStatus("WIFI_SUCCESS", "Connected to: " + WiFi.SSID());
  }
}

void createCSVHeader() {
  if (!sdCardAvailable) return;
  if (!SD.exists(dataFileName)) {
    File file = SD.open(dataFileName, FILE_WRITE);
    if (file) {
      file.print("Timestamp,");
      for (int i = 0; i < numRegisters; i++) {
        file.print(registersToRead[i].jsonKey);
        if (i < numRegisters - 1) file.print(",");
      }
      file.println();
      file.close();
    }
  }
}

void saveSystemStatus(const String& event, const String& message) {
  if (!sdCardAvailable) return;
  File file = SD.open(statusFileName, FILE_APPEND);
  if (!file) return;
  String timestamp = rtcAvailable ? getDateTimeString(rtc.now()) : String(millis());
  file.printf("%s - %s - %s\n", timestamp.c_str(), event.c_str(), message.c_str());
  file.close();
}

void printSystemInfo() {
  Serial.println("=== SYSTEM INFORMATION ===");
  Serial.print("🔌 Mode WiFi: "); Serial.println(WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
  Serial.print("💾 SD Card: "); Serial.println(sdCardAvailable ? "✅ Ready" : "❌ Error");
  if (rtcAvailable) {
    Serial.print("🕒 Waktu RTC Saat Ini: "); Serial.println(getDateTimeString(rtc.now()));
  }
  Serial.println("==================================");
}

String getDateTimeString(DateTime dt) {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}

float getFloatData(uint16_t address, const char* dataType) {
  int numRegsToRead = (strcmp(dataType, "U32") == 0) ? 2 : 1;
  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(address, numRegsToRead);
  if (result != node.ku8MBSuccess) return NAN;

  if (strcmp(dataType, "U32") == 0) {
    uint32_t value = ((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);
    return (float)value;
  }
  return NAN;
}