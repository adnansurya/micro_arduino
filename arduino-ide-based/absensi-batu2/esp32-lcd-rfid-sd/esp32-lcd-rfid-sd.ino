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
  
  // Upload data backup jika ada dan WiFi terhubung
  if (WiFi.status() == WL_CONNECTED && SD.begin(SS_PIN_SD)) {
    uploadBackupData();
  }
  
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

void uploadBackupData() {
  if (!SD.exists("/backup.csv")) {
    Serial.println("No backup file found.");
    return;
  }
  
  Serial.println("Found backup file, uploading data...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Uploading");
  lcd.setCursor(0, 1);
  lcd.print("Backup Data...");
  
  dataFile = SD.open("/backup.csv", FILE_READ);
  if (!dataFile) {
    Serial.println("Error opening backup file for reading");
    return;
  }
  
  // Skip header
  dataFile.readStringUntil('\n');
  
  int successCount = 0;
  int totalCount = 0;
  
  while (dataFile.available()) {
    String line = dataFile.readStringUntil('\n');
    line.trim();
    
    if (line.length() == 0) continue;
    
    // Parse CSV line: Tanggal,Waktu,UID,Foto_ID
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    int thirdComma = line.indexOf(',', secondComma + 1);
    
    if (firstComma == -1 || secondComma == -1 || thirdComma == -1) {
      Serial.println("Invalid backup line: " + line);
      continue;
    }
    
    String tanggal = line.substring(0, firstComma);
    String waktu = line.substring(firstComma + 1, secondComma);
    String uid = line.substring(secondComma + 1, thirdComma);
    String foto_id = line.substring(thirdComma + 1);
    
    Serial.println("Uploading backup: " + tanggal + " " + waktu + " " + uid);
    
    // Update LCD progress
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Backup: ");
    lcd.print(totalCount + 1);
    lcd.setCursor(0, 1);
    lcd.print("UID: ");
    lcd.print(uid.substring(0, 11));
    
    // Upload to cloud
    if (uploadBackupEntry(tanggal, waktu, uid, foto_id)) {
      successCount++;
      Serial.println("Backup entry uploaded successfully");
    } else {
      Serial.println("Failed to upload backup entry");
      // Jika gagal, stop proses dan biarkan data tetap di backup
      dataFile.close();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Backup Upload");
      lcd.setCursor(0, 1);
      lcd.print("Partial Success");
      delay(2000);
      return;
    }
    
    totalCount++;
    delay(500); // Delay antara upload untuk tidak membebani server
  }
  
  dataFile.close();
  
  // Jika semua data berhasil diupload, hapus file backup
  if (successCount == totalCount && totalCount > 0) {
    SD.remove("/backup.csv");
    Serial.println("Backup file deleted successfully");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Backup Complete");
    lcd.setCursor(0, 1);
    lcd.print("Deleted: ");
    lcd.print(totalCount);
  } else if (totalCount > 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Backup Partial");
    lcd.setCursor(0, 1);
    lcd.print("Success: ");
    lcd.print(successCount);
    lcd.print("/");
    lcd.print(totalCount);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No Backup Data");
    lcd.setCursor(0, 1);
    lcd.print("To Upload");
  }
  
  delay(3000);
}

bool uploadBackupEntry(String tanggal, String waktu, String uid, String foto_id) {
  HTTPClient http;
  http.begin(webAppURL);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON data untuk backup
  DynamicJsonDocument doc(1024);
  doc["tanggal"] = tanggal;
  doc["waktu"] = waktu;
  doc["uid"] = uid;
  doc["foto_id"] = foto_id;
  doc["keterangan"] = "Backup data from SD card";
  doc["is_backup"] = true; // Flag untuk menandai ini data backup
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("Uploading backup: ");
  Serial.println(jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  bool success = (httpResponseCode >= 100 && httpResponseCode < 400);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Backup HTTP Response: " + String(httpResponseCode));
    
    if (success) {
      Serial.println("Backup entry uploaded successfully");
    } else {
      Serial.println("Failed to upload backup entry, HTTP error: " + String(httpResponseCode));
    }
  } else {
    Serial.println("Error in backup HTTP request");
  }
  
  http.end();
  return success;
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