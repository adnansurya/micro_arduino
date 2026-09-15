#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include "SD.h"
#include "FS.h"
#include <EEPROM.h>
#include <Wire.h>
#include "RTClib.h"
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <time.h> // Library bawaan ESP32 untuk NTP

// Pin definitions
#define SS_PIN 4
#define RST_PIN 5
#define SD_CS_PIN 15

// LED Pin definitions
#define LED_MERAH 13
#define LED_KUNING 12
#define LED_HIJAU 14

// WiFi credentials (dipakai jika WiFiManager di-bypass)
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// Konfigurasi NTP Server (WITA / UTC+8)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600; // UTC+8 (8 Jam = 28800 Detik)
const int daylightOffset_sec = 0;     // Tidak ada daylight saving di Indonesia

// Google Apps Script URL
const char* scriptURL = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec";

// Components
MFRC522 mfrc522(SS_PIN, RST_PIN);
RTC_DS3231 rtc;
File dataFile;

// Variables
String currentUID = "";
String currentFotoID = "";

bool rtcAvailable = false;
bool timeSynced = false;
unsigned long lastTimeSync = 0;
const unsigned long TIME_SYNC_INTERVAL = 24 * 60 * 60 * 1000;  // Sync setiap 24 jam

WiFiManager wm;

// Declarations
void startupLEDSequence();
void initializeRTC();
void setupWifiManager();
void initializeRFID();
void initializeSD();
bool syncTimeFromNTP();
void processBackupData();
void setStandbyMode();
void readRFID();
void setLEDWarning();
void setLEDError();
void blinkLED(int ledPin, int count, int duration);
String getDateTimeString(DateTime dt);
String formatDate(DateTime dt);
String formatTime(DateTime dt);
String generateFotoID();
void processAttendance();
bool saveToLocalCSV(String date, String time, String uid, String fotoID);
bool saveToBackupCSV(String date, String time, String uid);
void sendToGoogleAppsScript(String date, String time, String uid, String fotoID);
bool sendBackupToGoogleAppsScript(String date, String time, String uid);
void sendDebug(String label, String error);
void triggerESPCam(String fotoID);
void createFileIfNotExists(const char* path);

void setup() {
  Serial.begin(115200);

  // Initialize LEDs
  pinMode(LED_MERAH, OUTPUT);
  pinMode(LED_KUNING, OUTPUT);
  pinMode(LED_HIJAU, OUTPUT);

  // Startup sequence - all LEDs blink twice
  startupLEDSequence();

  // Initialize components
  initializeRTC();
  setupWifiManager();
  SPI.begin();
  initializeRFID();
  initializeSD();

  // Sinkronisasi waktu menggunakan NTP jika WiFi terhubung
  if (rtcAvailable && WiFi.status() == WL_CONNECTED) {
    syncTimeFromNTP();
  }

  // Process backup data on startup
  blinkLED(LED_KUNING, 3, 300);
  processBackupData();

  // Tampilkan waktu terkini dari RTC sesaat sebelum masuk ke program utama
  if (rtcAvailable) {
    DateTime now = rtc.now();
    Serial.println("==========================================");
    Serial.print("🕒 WAKTU TERKINI SISTEM: ");
    Serial.println(getDateTimeString(now));
    Serial.println("==========================================");
  }

  Serial.println("Sistem Absensi IoT Ready");
  blinkLED(LED_HIJAU, 1, 300);

  // Standby mode - green LED on
  setStandbyMode();
}

void loop() {
  // Check for new RFID card
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    readRFID();
    delay(1000);
  }

  // Sync waktu periodic setiap 24 jam
  if (rtcAvailable && WiFi.status() == WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (!timeSynced || (currentMillis - lastTimeSync > TIME_SYNC_INTERVAL)) {
      syncTimeFromNTP();
    }
  }

  if (Serial.available() > 0) {
    String comms = Serial.readStringUntil('\n');
    if (comms == "reset-wifi") {
      wm.resetSettings();
      delay(1000);
      Serial.println("RESET WIFI SETTINGS");
      ESP.restart();
    }
  }
}

// Fungsi Sinkronisasi Waktu Utama Menggunakan NTP
bool syncTimeFromNTP() {
  if (!rtcAvailable) {
    Serial.println("❌ RTC tidak tersedia untuk sync waktu");
    return false;
  }

  Serial.println("🕒 Mensinkronisasi waktu dari NTP Server (pool.ntp.org)...");

  // Inisialisasi konfigurasi NTP di ESP32
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  // Tunggu hingga waktu berhasil diambil dari NTP (timeout 10 detik)
  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("❌ Gagal mendapatkan waktu dari NTP Server");
    sendDebug("syncTimeFromNTP", "Gagal mendapatkan waktu dari NTP");
    return false;
  }

  // Konversi struct tm dari NTP ke object DateTime
  DateTime ntpTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  // Set RTC dengan waktu dari NTP
  rtc.adjust(ntpTime);

  timeSynced = true;
  lastTimeSync = millis();

  Serial.print("✅ RTC Berhasil Disinkronisasi via NTP: ");
  Serial.println(getDateTimeString(ntpTime));
  return true;
}

void initializeSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Mount Failed");
    sendDebug("initializeSD", "SD Card Mount Failed");
    while(1){
      blinkLED(LED_MERAH, 1, 1500);
    }
  }
  Serial.println("SD Card Mounted");
  createFileIfNotExists("/absensi.csv");
  createFileIfNotExists("/backup.csv");
}

void initializeRTC() {
  if (rtc.begin()) {
    rtcAvailable = true;
    Serial.println("✅ RTC DS3231 terhubung");

    if (rtc.lostPower()) {
      Serial.println("⚠️ RTC kehilangan power, perlu sync waktu dari NTP");
      sendDebug("initializeRTC", "RTC kehilangan power, perlu sync waktu");
    } else {
      DateTime now = rtc.now();
      Serial.print("🕒 Waktu RTC saat ini: ");
      Serial.println(getDateTimeString(now));
    }
  } else {
    Serial.println("❌ Gagal terhubung ke RTC DS3231");
    sendDebug("initializeRTC", "Gagal terhubung ke RTC DS3231");
  }
}

void initializeRFID() {
  mfrc522.PCD_Init();
  Serial.println("RFID Reader Ready");
}

void createFileIfNotExists(const char* path) {
  if (!SD.exists(path)) {
    File file = SD.open(path, FILE_WRITE);
    if (file) {
      if (strcmp(path, "/absensi.csv") == 0) {
        file.println("Tanggal,Waktu,UID,Foto_ID");
      } else if (strcmp(path, "/backup.csv") == 0) {
        file.println("Tanggal,Waktu,UID");
      }
      file.close();
      Serial.println(String("Created file: ") + path);
    }
  }
}

void readRFID() {
  currentUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    currentUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    currentUID += String(mfrc522.uid.uidByte[i], HEX);
  }
  currentUID.toUpperCase();

  Serial.println("Card detected: " + currentUID);
  blinkLED(LED_HIJAU, 2, 300);

  currentFotoID = generateFotoID();
  processAttendance();
}

String generateFotoID() {
  String fotoID = "";
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

  randomSeed(micros());
  for (int i = 0; i < 10; i++) {
    fotoID += charset[random(0, strlen(charset))];
  }

  return fotoID;
}

void processAttendance() {
  DateTime now = rtc.now();
  String date = formatDate(now);
  String time = formatTime(now);

  bool sdSuccess = saveToLocalCSV(date, time, currentUID, currentFotoID);

  if (sdSuccess) {
    sendToGoogleAppsScript(date, time, currentUID, currentFotoID);
  }
}

String formatDate(DateTime dt) {
  char buffer[11];
  sprintf(buffer, "%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
  return String(buffer);
}

String formatTime(DateTime dt) {
  char buffer[9];
  sprintf(buffer, "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}

bool saveToLocalCSV(String date, String time, String uid, String fotoID) {
  dataFile = SD.open("/absensi.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.print(date); dataFile.print(",");
    dataFile.print(time); dataFile.print(",");
    dataFile.print(uid);  dataFile.print(",");
    dataFile.println(fotoID);
    dataFile.close();
    Serial.println("Data saved to absensi.csv");
    return true;
  } else {
    Serial.println("Error opening absensi.csv");
    sendDebug("saveToLocalCSV", "Error opening absensi.csv");
    setLEDError();
    return false;
  }
}

bool saveToBackupCSV(String date, String time, String uid) {
  dataFile = SD.open("/backup.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.print(date); dataFile.print(",");
    dataFile.print(time); dataFile.print(",");
    dataFile.println(uid);
    dataFile.close();
    Serial.println("Data saved to backup.csv");
    blinkLED(LED_KUNING, 3, 300);
    return true;
  } else {
    Serial.println("Error opening backup.csv");
    sendDebug("saveToBackupCSV", "Error opening backup.csv");
    setLEDError();
    return false;
  }
}

void sendToGoogleAppsScript(String date, String time, String uid, String fotoID) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptURL);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(4096);
    doc["date"] = date;
    doc["time"] = time;
    doc["uid"] = uid;
    doc["foto_id"] = fotoID;

    String payload;
    serializeJson(doc, payload);

    int httpResponseCode = http.POST(payload);
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > -1 && httpResponseCode < 400) {
      blinkLED(LED_HIJAU, 3, 300);
      triggerESPCam(fotoID);
    } else {
      setLEDWarning();
      saveToBackupCSV(date, time, uid);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected - saving to backup");
    DateTime now = rtc.now();
    setLEDWarning();
    saveToBackupCSV(formatDate(now), formatTime(now), currentUID);
  }

  delay(1000);
  setStandbyMode();
}

void sendDebug(String label, String error) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptURL);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(4096);
    doc["action"] = "debug";
    doc["label"] = label;
    doc["error"] = error;

    String payload;
    serializeJson(doc, payload);
    http.POST(payload);
    http.end();
  }
}

void triggerESPCam(String fotoID) {
  HTTPClient http;
  String url = "http://esp32cam.local/capture?foto_id=" + fotoID;
  http.begin(url);
  http.GET();
  http.end();
}

void processBackupData() {
  if (!SD.exists("/backup.csv")) return;

  File backupFile = SD.open("/backup.csv", FILE_READ);
  if (!backupFile) return;

  int backupCount = 0;
  int successCount = 0;

  while (backupFile.available()) {
    String line = backupFile.readStringUntil('\n');
    line.trim();

    if (line.length() > 0 && line.indexOf("Tanggal,Waktu,UID") == -1) {
      backupCount++;
      int firstComma = line.indexOf(',');
      int secondComma = line.indexOf(',', firstComma + 1);

      if (firstComma != -1 && secondComma != -1) {
        String date = line.substring(0, firstComma);
        String time = line.substring(firstComma + 1, secondComma);
        String uid = line.substring(secondComma + 1);

        if (sendBackupToGoogleAppsScript(date, time, uid)) {
          successCount++;
          blinkLED(LED_HIJAU, 1, 200);
        } else {
          blinkLED(LED_KUNING, 1, 200);
        }
      }
    }
  }
  backupFile.close();

  if (successCount == backupCount && backupCount > 0) {
    SD.remove("/backup.csv");
    blinkLED(LED_HIJAU, 3, 300);
  } else if (backupCount > 0) {
    setLEDWarning();
    delay(2000);
  }
}

bool sendBackupToGoogleAppsScript(String date, String time, String uid) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(scriptURL);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(2048);
  doc["date"] = date;
  doc["time"] = time;
  doc["uid"] = uid;
  doc["foto_id"] = "BACKUP_" + generateFotoID();

  String payload;
  serializeJson(doc, payload);

  int httpResponseCode = http.POST(payload);
  http.end();

  return (httpResponseCode > -1 && httpResponseCode < 400);
}

void startupLEDSequence() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_MERAH, HIGH); digitalWrite(LED_KUNING, HIGH); digitalWrite(LED_HIJAU, HIGH);
    delay(300);
    digitalWrite(LED_MERAH, LOW); digitalWrite(LED_KUNING, LOW); digitalWrite(LED_HIJAU, LOW);
    delay(300);
  }
}

void setStandbyMode() {
  digitalWrite(LED_MERAH, LOW);
  digitalWrite(LED_KUNING, LOW);
  digitalWrite(LED_HIJAU, HIGH);
}

void blinkLED(int ledPin, int count, int duration) {
  for (int i = 0; i < count; i++) {
    digitalWrite(ledPin, HIGH);
    delay(duration);
    digitalWrite(ledPin, LOW);
    if (i < count - 1) delay(duration);
  }
}

void setLEDWarning() {
  digitalWrite(LED_MERAH, LOW);
  digitalWrite(LED_KUNING, HIGH);
  digitalWrite(LED_HIJAU, LOW);
}

void setLEDError() {
  digitalWrite(LED_MERAH, HIGH);
  digitalWrite(LED_KUNING, LOW);
  digitalWrite(LED_HIJAU, LOW);
}

void setupWifiManager() {
  wm.setConfigPortalTimeout(60);
  wm.autoConnect("ESP32 Absensi");
}

String getDateTimeString(DateTime dt) {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          dt.year(), dt.month(), dt.day(),
          dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}