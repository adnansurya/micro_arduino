#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#include <SD.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Pin definitions
#define SS_PIN_RFID 4
#define RST_PIN_RFID 5
#define SS_PIN_SD 13

// WiFi Credentials
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// Google Apps Script Web App URL
const char* webAppURL = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec";

// ESP32-CAM URL
const char* esp32camURL = "http://esp32cam.local/foto_id";

// Initialize devices
MFRC522 mfrc522(SS_PIN_RFID, RST_PIN_RFID);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// File object
File dataFile;

// Variables
String currentDate;
String currentTime;
String lastCardUID = "";
String todayDate = "";
String currentFotoID = ""; // Untuk menyimpan foto_id terakhir

// Fungsi untuk generate random string di ESP32
String generateRandomString(int length) {
  String result = "";
  String characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int charactersLength = characters.length();
  
  for (int i = 0; i < length; i++) {
    int randomIndex = random(charactersLength);
    result += characters[randomIndex];
  }
  return result;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Sistem Absensi Sekolah ESP32");
  
  // Initialize random seed
  randomSeed(analogRead(0));
  
  // Initialize SPI
  SPI.begin();
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  
  // Initialize RFID
  mfrc522.PCD_Init();
  Serial.println("RFID Reader Initialized");
  
  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    lcd.setCursor(0, 1);
    lcd.print("RTC Error!     ");
    while (1);
  }
  
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // Initialize SD Card
  Serial.print("Initializing SD card...");
  lcd.setCursor(0, 1);
  lcd.print("SD Card...      ");
  
  if (!SD.begin(SS_PIN_SD)) {
    Serial.println("SD Card initialization failed!");
    lcd.setCursor(0, 1);
    lcd.print("SD Card Error!  ");
  } else {
    Serial.println("SD Card initialized.");
    createCSVHeader();
    createBackupHeader();
  }
  
  // Connect to WiFi
  connectToWiFi();
  
  // Get today's date
  updateDateTime();
  todayDate = currentDate;
  
  // Display ready message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Absensi Sekolah");
  lcd.setCursor(0, 1);
  lcd.print("Tempelkan Kartu");
  
  Serial.println("System Ready!");
}

void loop() {
  // Update date and time every second
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    updateDateTime();
    
    if (currentDate != todayDate) {
      todayDate = currentDate;
      Serial.println("New day detected");
    }
    
    lastUpdate = millis();
  }
  
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  
  // Read card UID
  String cardUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    cardUID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    cardUID += String(mfrc522.uid.uidByte[i], HEX);
  }
  cardUID.toUpperCase();
  
  // Check if it's the same card (debounce)
  if (cardUID == lastCardUID) {
    mfrc522.PICC_HaltA();
    return;
  }
  
  lastCardUID = cardUID;
  
  Serial.print("Card UID: ");
  Serial.println(cardUID);
  
  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID: ");
  lcd.print(cardUID.substring(0, 11));
  lcd.setCursor(0, 1);
  lcd.print(cardUID.substring(11));
  
  // Save to cloud and SD card
  processAttendance(cardUID);
  
  mfrc522.PICC_HaltA();
}

void processAttendance(String uid) {
  bool cloudSuccess = false;
  bool sdSuccess = false;
  currentFotoID = generateRandomString(8); // Generate foto_id di ESP32
  
  // Try to save to cloud first
  if (WiFi.status() == WL_CONNECTED) {
    cloudSuccess = saveToCloud(uid);
    
    // Jika berhasil kirim ke cloud, kirim foto_id ke ESP32-CAM
    if (cloudSuccess) {
      bool camSuccess = sendFotoIDToCamera();
      
      // Tampilkan hasil di LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cloud: OK!     ");
      
      if (camSuccess) {
        lcd.setCursor(0, 1);
        lcd.print("Camera: OK!    ");
        Serial.println("Foto ID sent to camera successfully");
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Camera: Failed ");
        Serial.println("Failed to send foto ID to camera");
      }
    } else {
      // Jika gagal kirim ke cloud, simpan ke backup
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Cloud: Failed  ");
      lcd.setCursor(0, 1);
      lcd.print("Saved Backup   ");
      saveToBackup(uid);
    }
  } else {
    // Jika WiFi tidak terhubung, simpan ke backup
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Offline   ");
    lcd.setCursor(0, 1);
    lcd.print("Saved Backup   ");
    saveToBackup(uid);
  }
  
  // Selalu simpan ke SD card utama juga
  if (SD.begin(SS_PIN_SD)) {
    sdSuccess = saveToSD(uid);
  }
  
  delay(3000);
  
  // Reset display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Absensi Sekolah");
  lcd.setCursor(0, 1);
  lcd.print("Tempelkan Kartu");
}

bool saveToCloud(String uid) {
  HTTPClient http;
  http.begin(webAppURL);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON data dengan foto_id yang sudah digenerate
  DynamicJsonDocument doc(1024);
  doc["tanggal"] = currentDate;
  doc["waktu"] = currentTime;
  doc["uid"] = uid;
  doc["foto_id"] = currentFotoID; // Kirim foto_id yang sudah digenerate
  doc["keterangan"] = "Auto-generated from ESP32";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("Sending to cloud: ");
  Serial.println(jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  // Anggap berhasil jika response code 1xx, 2xx, atau 3xx
  bool success = (httpResponseCode >= 100 && httpResponseCode < 400);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Response: " + String(httpResponseCode));
    Serial.println("Response: " + response);
    
    if (success) {
      Serial.println("Data saved to cloud successfully");
    } else {
      Serial.println("Failed to save to cloud, HTTP error: " + String(httpResponseCode));
    }
    
    http.end();
    return success;
  } else {
    Serial.println("Error in HTTP request");
    http.end();
    return false;
  }
}

bool sendFotoIDToCamera() {
  HTTPClient http;
  http.begin(esp32camURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  String postData = "foto_id=" + currentFotoID + "&uid=" + lastCardUID;
  
  int httpResponseCode = http.POST(postData);
  bool success = (httpResponseCode >= 100 && httpResponseCode < 400);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Camera HTTP Response: " + String(httpResponseCode));
    Serial.println("Camera Response: " + response);
  } else {
    Serial.println("Error in camera HTTP request");
  }
  
  http.end();
  return success;
}

bool saveToSD(String uid) {
  dataFile = SD.open("/absensi.csv", FILE_APPEND);
  
  if (dataFile) {
    dataFile.print(currentDate);
    dataFile.print(",");
    dataFile.print(currentTime);
    dataFile.print(",");
    dataFile.print(uid);
    dataFile.print(",");
    dataFile.println(currentFotoID);
    
    dataFile.close();
    Serial.println("Data saved to SD card");
    return true;
  } else {
    Serial.println("Error opening SD file");
    return false;
  }
}

bool saveToBackup(String uid) {
  dataFile = SD.open("/backup.csv", FILE_APPEND);
  
  if (dataFile) {
    dataFile.print(currentDate);
    dataFile.print(",");
    dataFile.print(currentTime);
    dataFile.print(",");
    dataFile.print(uid);
    dataFile.print(",");
    dataFile.println(currentFotoID);
    
    dataFile.close();
    Serial.println("Data saved to backup file");
    return true;
  } else {
    Serial.println("Error opening backup file");
    return false;
  }
}

void connectToWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP().toString().substring(0, 12));
  } else {
    Serial.println("\nFailed to connect to WiFi");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!   ");
    lcd.setCursor(0, 1);
    lcd.print("Using SD Only  ");
  }
  
  delay(2000);
}

void updateDateTime() {
  DateTime now = rtc.now();
  
  currentDate = "";
  currentDate += now.year();
  currentDate += "-";
  if (now.month() < 10) currentDate += "0";
  currentDate += now.month();
  currentDate += "-";
  if (now.day() < 10) currentDate += "0";
  currentDate += now.day();
  
  currentTime = "";
  if (now.hour() < 10) currentTime += "0";
  currentTime += now.hour();
  currentTime += ":";
  if (now.minute() < 10) currentTime += "0";
  currentTime += now.minute();
  currentTime += ":";
  if (now.second() < 10) currentTime += "0";
  currentTime += now.second();
}

void createCSVHeader() {
  if (!SD.exists("/absensi.csv")) {
    dataFile = SD.open("/absensi.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Tanggal,Waktu,UID,Foto_ID");
      dataFile.close();
      Serial.println("Created absensi.csv with header");
    }
  }
}

void createBackupHeader() {
  if (!SD.exists("/backup.csv")) {
    dataFile = SD.open("/backup.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Tanggal,Waktu,UID,Foto_ID");
      dataFile.close();
      Serial.println("Created backup.csv with header");
    }
  }
}