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

// Pin definitions
#define SS_PIN 4
#define RST_PIN 5
#define SD_CS_PIN 15

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



  // Initialize components

  initializeRTC();

  connectWiFi();
  SPI.begin();
  initializeRFID();
  initializeSD();

  Serial.println("Sistem Absensi IoT Ready");
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
  saveToLocalCSV(date, time, currentUID, currentFotoID);

  // Send to Google Apps Script
  sendToGoogleAppsScript(date, time, currentUID, currentFotoID);
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

void saveToLocalCSV(String date, String time, String uid, String fotoID) {
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
  } else {
    Serial.println("Error opening absensi.csv");
  }
}

void saveToBackupCSV(String date, String time, String uid) {
  dataFile = SD.open("/backup.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.print(date);
    dataFile.print(",");
    dataFile.print(time);
    dataFile.print(",");
    dataFile.println(uid);
    dataFile.close();
    Serial.println("Data saved to backup.csv");
  } else {
    Serial.println("Error opening backup.csv");
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
      // Success - trigger ESP32-CAM to take photo
      triggerESPCam(fotoID);
    } else {
      // Failed - save to backup
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
}

void triggerESPCam(String fotoID) {
  HTTPClient http;
  String url = "http://esp32cam.local/" + fotoID;

  http.begin(url);
  int httpResponseCode = http.GET();

  Serial.print("ESP32-CAM trigger response: ");
  Serial.println(httpResponseCode);

  http.end();
}

String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;

  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }

  return encodedString;
}