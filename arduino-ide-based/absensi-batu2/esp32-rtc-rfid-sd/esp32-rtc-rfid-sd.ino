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

// Pin definitions
#define SS_PIN 4
#define RST_PIN 5
#define SD_CS_PIN 15

// LED Pin definitions
#define LED_MERAH 13
#define LED_KUNING 12
#define LED_HIJAU 14

// WiFi credentials
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// Google Apps Script URL
const char* scriptURL = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec";

// Components
MFRC522 mfrc522(SS_PIN, RST_PIN);
RTC_DS3231 rtc;
File dataFile;

// Variables
String currentUID = "";
String currentFotoID = "";

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

  // Process backup data on startup
  blinkLED(LED_KUNING, 3, 300);
  processBackupData();

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
}

void initializeSD() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  Serial.println("SD Card Mounted");

  // Create files if they don't exist
  createFileIfNotExists("/absensi.csv");
  createFileIfNotExists("/backup.csv");
}

void initializeRTC() {
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void initializeRFID() {
  mfrc522.PCD_Init();
  Serial.println("RFID Reader Ready");
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
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

  // Card detected - green LED blink once
  blinkLED(LED_HIJAU, 2, 300);

  // Generate unique Foto ID
  currentFotoID = generateFotoID();

  // Save to SD card and send to server
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

  // Save to local CSV
  bool sdSuccess = saveToLocalCSV(date, time, currentUID, currentFotoID);

  // Send to Google Apps Script
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
    dataFile.print(date);
    dataFile.print(",");
    dataFile.print(time);
    dataFile.print(",");
    dataFile.print(uid);
    dataFile.print(",");
    dataFile.println(fotoID);
    dataFile.close();
    Serial.println("Data saved to absensi.csv");
    return true;
  } else {
    Serial.println("Error opening absensi.csv");
    // Failed to save to backup - red LED on
    setLEDError();
    return false;
  }
}

bool saveToBackupCSV(String date, String time, String uid) {
  dataFile = SD.open("/backup.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.print(date);
    dataFile.print(",");
    dataFile.print(time);
    dataFile.print(",");
    dataFile.println(uid);
    dataFile.close();
    Serial.println("Data saved to backup.csv");
    // Success saving to backup - yellow LED blink 3 times
    blinkLED(LED_KUNING, 3, 300);
    return true;
  } else {
    Serial.println("Error opening backup.csv");
    // Failed to save to backup - red LED on
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

    Serial.println("Payload size: " + String(payload.length()));

    int httpResponseCode = http.POST(payload);

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode < 400) {
      // Success - trigger ESP32-CAM to take photo and green LED blink 3 times
      blinkLED(LED_HIJAU, 3, 300);
      triggerESPCam(fotoID);
    } else {
      // Failed - yellow LED on
      setLEDWarning();
      saveToBackupCSV(date, time, uid);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected - saving to backup");
    DateTime now = rtc.now();
    String date = formatDate(now);
    String time = formatTime(now);
    saveToBackupCSV(date, time, currentUID);
  }

  // Return to standby mode
  delay(1000);
  setStandbyMode();
}

void triggerESPCam(String fotoID) {
  HTTPClient http;
  String url = "http://esp32cam.local/capture?foto_id=" + fotoID;

  http.begin(url);
  int httpResponseCode = http.GET();

  Serial.print("ESP32-CAM trigger response: ");
  Serial.println(httpResponseCode);

  http.end();
}

// Backup Data Processing Functions
void processBackupData() {
  Serial.println("Checking for backup data...");

  if (!SD.exists("/backup.csv")) {
    Serial.println("No backup file found");
    return;
  }

  File backupFile = SD.open("/backup.csv", FILE_READ);
  if (!backupFile) {
    Serial.println("Failed to open backup.csv");
    return;
  }

  // Skip header line
  backupFile.readStringUntil('\n');

  int backupCount = 0;
  int successCount = 0;

  // Read backup data line by line
  while (backupFile.available()) {
    String line = backupFile.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      backupCount++;
      Serial.println("Processing backup: " + line);

      // Parse CSV line
      int firstComma = line.indexOf(',');
      int secondComma = line.indexOf(',', firstComma + 1);

      if (firstComma != -1 && secondComma != -1) {
        String date = line.substring(0, firstComma);
        String time = line.substring(firstComma + 1, secondComma);
        String uid = line.substring(secondComma + 1);

        if (sendBackupToGoogleAppsScript(date, time, uid)) {
          successCount++;
          // Green LED blink for each successful backup upload
          blinkLED(LED_HIJAU, 1, 200);
        } else {
          // Yellow LED blink for failed backup upload
          blinkLED(LED_KUNING, 1, 200);
        }
      }
    }
  }

  backupFile.close();

  Serial.println("Backup processing completed: " + String(successCount) + "/" + String(backupCount) + " successful");

  // If all backups were successfully sent, delete the backup file
  if (successCount == backupCount && backupCount > 0) {
    if (SD.remove("/backup.csv")) {
      Serial.println("Backup file deleted successfully");
      // Green LED blink 3 times for successful cleanup
      blinkLED(LED_HIJAU, 3, 300);
    } else {
      Serial.println("Failed to delete backup file");
      // Red LED blink for deletion error
      blinkLED(LED_MERAH, 2, 300);
    }
  } else if (backupCount > 0) {
    Serial.println("Some backups failed to send, keeping backup file");
    // Yellow LED on for partial success
    setLEDWarning();
    delay(2000);
  }
}

bool sendBackupToGoogleAppsScript(String date, String time, String uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for backup upload");
    return false;
  }

  HTTPClient http;
  http.begin(scriptURL);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(2048);
  doc["date"] = date;
  doc["time"] = time;
  doc["uid"] = uid;
  doc["foto_id"] = "BACKUP_" + generateFotoID();  // Special foto_id for backups

  String payload;
  serializeJson(doc, payload);

  Serial.println("Sending backup data: " + payload);

  int httpResponseCode = http.POST(payload);
  Serial.print("Backup HTTP Response code: ");
  Serial.println(httpResponseCode);

  http.end();

  return (httpResponseCode < 400);
}

// LED Control Functions
void startupLEDSequence() {
  // All LEDs blink twice
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_MERAH, HIGH);
    digitalWrite(LED_KUNING, HIGH);
    digitalWrite(LED_HIJAU, HIGH);
    delay(300);
    digitalWrite(LED_MERAH, LOW);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_HIJAU, LOW);
    delay(300);
  }
}

void setStandbyMode() {
  // Turn off all LEDs except green
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
  // Yellow LED on, others off
  digitalWrite(LED_MERAH, LOW);
  digitalWrite(LED_KUNING, HIGH);
  digitalWrite(LED_HIJAU, LOW);
}

void setLEDError() {
  // Red LED on, others off
  digitalWrite(LED_MERAH, HIGH);
  digitalWrite(LED_KUNING, LOW);
  digitalWrite(LED_HIJAU, LOW);
}

void generalError() {
  // Red LED blink 3 times for general errors
  blinkLED(LED_MERAH, 3, 300);
}

void setupWifiManager() {
  WiFiManager wm;

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("ESP32 Absensi", "password123");  // password protected ap

  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }
}
