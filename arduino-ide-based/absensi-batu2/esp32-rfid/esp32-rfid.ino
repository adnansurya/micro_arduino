#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <RTClib.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>  // Library untuk LCD I2C

// Shared SPI Pins
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

// Chip Select Pins
#define SS_RFID 4
#define SS_SD 13
#define RST_RFID 5

// LCD Configuration
#define LCD_ADDRESS 0x27  // Alamat I2C LCD (bisa 0x3F juga)
#define LCD_COLS 16
#define LCD_ROWS 2

MFRC522 mfrc522(SS_RFID, RST_RFID);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

const char* APPS_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec";
const char* ESP32CAM_URL = "http://esp32cam.local/api/take_photo";
const char* TIME_API_URL = "http://worldtimeapi.org/api/timezone/Asia/Makassar";

// WiFi credentials
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// OTA Configuration
const char* OTA_hostname = "esp32-attendance";
const char* OTA_password = "admin123";

bool isOnline = false;
bool sdCardAvailable = false;
bool rtcSynced = false;
bool backupProcessed = false;
bool systemReady = false;
bool otaEnabled = true;

// State machine variables
enum SystemState {
  STATE_STARTUP,
  STATE_BACKUP_SYNC,
  STATE_READY,
  STATE_PROCESSING_RFID,
  STATE_SENDING_DATA,
  STATE_OTA_UPDATE
};

SystemState currentState = STATE_STARTUP;

// Timing variables
unsigned long lastStateChange = 0;
unsigned long lastRfidCheck = 0;
unsigned long lastRtcSync = 0;
unsigned long lastOtaHandle = 0;
unsigned long lastDisplayUpdate = 0;

// RFID debouncing
unsigned long lastRfidRead = 0;
bool rfidProcessing = false;
String currentUid = "";

// LCD display variables
unsigned long displayTimeout = 0;
String line1 = "";
String line2 = "";

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 Attendance System...");
  
  // Initialize LCD
  initLCD();
  
  // Startup sequence
  changeState(STATE_STARTUP);
  
  // Initialize devices
  initSPI();
  initRFID();
  initSDCard();
  initRTC();
  initWiFi();
  
  // Initialize OTA
  if (otaEnabled) {
    initOTA();
  }
  
  // Synchronize RTC with internet time
  if (isOnline) {
    changeState(STATE_BACKUP_SYNC);
    syncRTCTime();
  } else {
    displayMessage("RTC: No Internet", "Sync Failed");
    delay(2000);
  }
  
  // Create CSV headers
  createCSVHeaders();
  
  // Process backup data at startup
  if (isOnline && sdCardAvailable) {
    processBackupDataAtStartup();
  }
  
  // System ready
  changeState(STATE_READY);
  systemReady = true;
  Serial.println("=== SYSTEM READY ===");
  printSystemStatus();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Handle OTA updates
  if (otaEnabled && currentMillis - lastOtaHandle > 100) {
    ArduinoOTA.handle();
    lastOtaHandle = currentMillis;
  }
  
  // Update LCD display periodically
  if (currentMillis - lastDisplayUpdate > 500) {
    updateDisplay();
    lastDisplayUpdate = currentMillis;
  }
  
  // State machine
  switch (currentState) {
    case STATE_STARTUP:
      // Initialization handled in setup()
      break;
      
    case STATE_BACKUP_SYNC:
      // Backup processing handled in setup()
      break;
      
    case STATE_READY:
      // Check for RFID cards (non-blocking)
      if (currentMillis - lastRfidCheck > 100 && !rfidProcessing) {
        checkRFID();
        lastRfidCheck = currentMillis;
      }
      
      // Periodic RTC sync (every 1 hour)
      if (isOnline && currentMillis - lastRtcSync > 3600000) {
        lastRtcSync = currentMillis;
        Serial.println("Performing periodic RTC sync...");
        syncRTCTime();
      }
      break;
      
    case STATE_PROCESSING_RFID:
      // RFID processing is handled asynchronously
      // Check if processing is complete
      if (!rfidProcessing) {
        changeState(STATE_READY);
      }
      break;
      
    case STATE_SENDING_DATA:
      // Data sending handled asynchronously
      // Kembali ke READY setelah delay
      if (currentMillis - lastStateChange > 3000) { // 3 detik di state SENDING_DATA
        changeState(STATE_READY);
      }
      break;
      
    case STATE_OTA_UPDATE:
      // OTA handled by ArduinoOTA
      break;
  }
}

// ==================== LCD FUNCTIONS ====================

void initLCD() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  displayMessage("System Startup", "Initializing...");
  Serial.println("✓ LCD Initialized");
}

void updateDisplay() {
  switch (currentState) {
    case STATE_STARTUP:
      displayMessage("System Startup", "Please wait...");
      break;
      
    case STATE_BACKUP_SYNC:
      displayMessage("Backup Sync", "Processing...");
      break;
      
    case STATE_READY:
      {
        DateTime now = rtc.now();
        String timeStr = formatTime(now);
        String dateStr = formatDate(now);
        
        String statusLine = "Ready";
        if (!isOnline) statusLine = "Offline";
        
        String line1 = timeStr + " " + statusLine;
        String line2 = dateStr;
        
        displayMessage(line1, line2);
      }
      break;
      
    case STATE_PROCESSING_RFID:
      displayMessage("Processing", "RFID Card...");
      break;
      
    case STATE_SENDING_DATA:
      displayMessage("Sending Data", "To Cloud...");
      break;
      
    case STATE_OTA_UPDATE:
      displayMessage("OTA Update", "In Progress...");
      break;
  }
}

void displayMessage(String line1, String line2) {
  lcd.clear();
  
  // Center align line 1 - PERBAIKAN: gunakan casting yang tepat
  int startPos1 = max(0, (int)((LCD_COLS - line1.length()) / 2));
  lcd.setCursor(startPos1, 0);
  lcd.print(line1);
  
  // Center align line 2 - PERBAIKAN: gunakan casting yang tepat
  int startPos2 = max(0, (int)((LCD_COLS - line2.length()) / 2));
  lcd.setCursor(startPos2, 1);
  lcd.print(line2);
}

void displayCardInfo(String uid, String time, String status) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID: " + uid.substring(0, 8));
  
  lcd.setCursor(0, 1);
  lcd.print(time + " " + status);
}

// ==================== STATE MANAGEMENT ====================

void changeState(SystemState newState) {
  currentState = newState;
  lastStateChange = millis();
  
  Serial.print("State changed to: ");
  switch (newState) {
    case STATE_STARTUP: 
      Serial.println("STARTUP");
      displayMessage("System Startup", "Initializing...");
      break;
    case STATE_BACKUP_SYNC: 
      Serial.println("BACKUP_SYNC");
      displayMessage("Backup Sync", "Please wait...");
      break;
    case STATE_READY: 
      Serial.println("READY");
      displayMessage("System Ready", "Tap RFID Card");
      delay(1000); // Kurangi delay
      break;
    case STATE_PROCESSING_RFID: 
      Serial.println("PROCESSING_RFID");
      displayMessage("RFID Detected", "Processing...");
      break;
    case STATE_SENDING_DATA: 
      Serial.println("SENDING_DATA");
      displayMessage("Sending Data", "To Cloud...");
      break;
    case STATE_OTA_UPDATE: 
      Serial.println("OTA_UPDATE");
      displayMessage("OTA Update", "In Progress...");
      break;
  }
}

// ==================== RFID HANDLING ====================

void checkRFID() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    if (millis() - lastRfidRead > 1000) { // Debouncing 1 detik
      lastRfidRead = millis();
      processRFIDCardAsync();
    } else {
      mfrc522.PICC_HaltA();
    }
  }
}

void processRFIDCardAsync() {
  rfidProcessing = true;
  changeState(STATE_PROCESSING_RFID);
  
  currentUid = getRFIDUID();
  DateTime now = rtc.now();
  String timestamp = formatTimestamp(now);
  String date = formatDate(now);
  String time = formatTime(now);
  
  // Display card info immediately
  displayCardInfo(currentUid, time, "Processing");
  
  Serial.println("\n=== RFID Card Detected ===");
  Serial.println("UID: " + currentUid);
  Serial.println("Date: " + date);
  Serial.println("Time: " + time);
  
  // Process in background - gunakan state machine
  processRFIDData(currentUid, timestamp, date, time);
  
  mfrc522.PICC_HaltA();
}

void processRFIDData(String uid, String timestamp, String date, String time) {
  String jsonData = createJSONData(uid, timestamp, date, time);
  
  if (isOnline) {
    changeState(STATE_SENDING_DATA);
    
    String rowId = sendToGoogleAppsScript(jsonData);
    
    if (rowId != "") {
      displayMessage("Data Sent", "Success!");
      Serial.println("✓ Data sent to Google Sheets, Row ID: " + rowId);
      
      // Trigger ESP32-CAM
      if (triggerESP32CAM(uid, timestamp, rowId)) {
        displayMessage("Photo Taken", "Uploading...");
        Serial.println("✓ ESP32-CAM triggered successfully");
        
        // Save to attendance.csv
        String csvData = createCSVData(uid, timestamp, date, time, rowId, "PENDING");
        saveToCSV("/attendance.csv", csvData);
        
        displayMessage("Complete", "Photo Uploaded!");
      } else {
        displayMessage("Camera Error", "Data Saved");
        Serial.println("✗ Failed to trigger ESP32-CAM");
        
        String csvData = createCSVData(uid, timestamp, date, time, rowId, "FAILED");
        saveToCSV("/attendance.csv", csvData);
      }
    } else {
      displayMessage("Send Failed", "Saved to Backup");
      Serial.println("✗ Failed to send to Google Sheets");
      
      String csvData = createCSVData(uid, timestamp, date, time, "", "");
      saveToCSV("/backup.csv", csvData);
    }
  } else {
    displayMessage("Offline Mode", "Saved to Backup");
    Serial.println("⚠ Offline - data saved to backup");
    
    String csvData = createCSVData(uid, timestamp, date, time, "", "");
    saveToCSV("/backup.csv", csvData);
  }
  
  // Tandai processing selesai
  rfidProcessing = false;
  Serial.println("=== Processing Complete ===\n");
  
  // Tidak perlu delay di sini, biarkan state machine yang mengatur
}

// ==================== OTA SETUP ====================

void initOTA() {
  ArduinoOTA.setHostname(OTA_hostname);
  ArduinoOTA.setPassword(OTA_password);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
    changeState(STATE_OTA_UPDATE);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update Complete");
    displayMessage("OTA Complete", "Rebooting...");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percentage = (progress / (total / 100));
    String progressStr = "Progress: " + String(percentage) + "%";
    displayMessage("OTA Update", progressStr);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    String errorMsg = "Error: " + String(error);
    displayMessage("OTA Failed", errorMsg);
    delay(3000);
    changeState(STATE_READY);
  });
  
  ArduinoOTA.begin();
  Serial.println("✓ OTA Update Ready");
  displayMessage("OTA Ready", OTA_hostname);
  delay(2000);
}

// ==================== NETWORK FUNCTIONS ====================

String sendToGoogleAppsScript(String jsonData) {
  HTTPClient http;
  http.begin(APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);
  
  int httpCode = http.POST(jsonData);
  String rowId = "";
  
  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Google Apps Script Response: " + response);
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc["status"] == "success") {
      rowId = doc["row_id"] | "";
    }
  } else {
    Serial.println("HTTP Error: " + String(httpCode));
  }
  
  http.end();
  return rowId;
}

bool triggerESP32CAM(String uid, String timestamp, String rowId) {
  HTTPClient http;
  http.begin(ESP32CAM_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  
  DynamicJsonDocument doc(512);
  doc["card_uid"] = uid;
  doc["timestamp"] = timestamp;
  doc["row_id"] = rowId;
  doc["device_id"] = "ESP32_ATTENDANCE_01";
  
  String payload;
  serializeJson(doc, payload);
  
  int httpCode = http.POST(payload);
  bool success = (httpCode == 200);
  
  if (success) {
    String response = http.getString();
    Serial.println("ESP32-CAM Response: " + response);
  } else {
    Serial.println("ESP32-CAM HTTP Error: " + String(httpCode));
  }
  
  http.end();
  return success;
}

// ==================== DEVICE INITIALIZATION ====================

void initSPI() {
  Serial.println("Initializing Shared SPI Bus...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  Serial.println("✓ Shared SPI Bus Initialized");
}

void initRFID() {
  Serial.println("Initializing RFID Reader...");
  pinMode(SS_RFID, OUTPUT);
  digitalWrite(SS_RFID, HIGH);
  mfrc522.PCD_Init();
  
  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.printf("RFID Firmware Version: 0x%02X", version);
  
  if (version == 0x91 || version == 0x92) {
    Serial.println(" - MFRC522 Detected ✓");
  } else {
    Serial.println(" - Unknown RFID Chip ✗");
  }
}

void initSDCard() {
  Serial.println("Initializing SD Card...");
  pinMode(SS_SD, OUTPUT);
  digitalWrite(SS_SD, HIGH);
  
  if (!SD.begin(SS_SD, SPI)) {
    Serial.println("✗ SD Card initialization failed!");
    sdCardAvailable = false;
    displayMessage("SD Card", "Init Failed!");
    delay(2000);
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("✗ No SD Card detected");
    sdCardAvailable = false;
    displayMessage("No SD Card", "Detected!");
    delay(2000);
    return;
  }
  
  Serial.print("✓ SD Card Type: ");
  switch(cardType) {
    case CARD_MMC: Serial.println("MMC"); break;
    case CARD_SD: Serial.println("SDSC"); break;
    case CARD_SDHC: Serial.println("SDHC"); break;
    default: Serial.println("UNKNOWN"); break;
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("✓ SD Card Size: %lluMB\n", cardSize);
  sdCardAvailable = true;
  displayMessage("SD Card", "Initialized!");
  delay(1000);
}

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("✗ RTC not found!");
    displayMessage("RTC Error", "Not Found!");
    delay(2000);
    return;
  }
  Serial.println("✓ RTC Found");
  
  DateTime now = rtc.now();
  Serial.printf("RTC Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
  displayMessage("RTC", "Initialized!");
  delay(1000);
}

void initWiFi() {
  displayMessage("WiFi", "Connecting...");
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < 20000) {
    Serial.print(".");
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    isOnline = true;
    Serial.println("\n✓ WiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    displayMessage("WiFi Connected", WiFi.localIP().toString());
    delay(2000);
  } else {
    isOnline = false;
    Serial.println("\n✗ WiFi Failed - Offline Mode");
    displayMessage("WiFi Failed", "Offline Mode");
    delay(2000);
  }
}

void syncRTCTime() {
  if (!isOnline) {
    Serial.println("⚠ Cannot sync RTC - No internet connection");
    return;
  }
  
  Serial.println("Synchronizing RTC with internet time...");  
  
  HTTPClient http;
  http.begin(TIME_API_URL);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String datetimeStr = doc["datetime"];
      unsigned long unixtime = doc["unixtime"];
      
      Serial.println("Internet Time: " + datetimeStr);
      Serial.println("Unix Time: " + String(unixtime));
      
      // Convert Unix time to DateTime
      DateTime internetTime(unixtime);
      
      // Set RTC time
      rtc.adjust(internetTime);
      rtcSynced = true;
      
      Serial.println("✓ RTC Synchronized Successfully");
      Serial.printf("RTC Updated Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    internetTime.year(), internetTime.month(), internetTime.day(),
                    internetTime.hour(), internetTime.minute(), internetTime.second());
      
    } else {
      Serial.println("✗ Failed to parse time API response");      
    }
  } else {
    Serial.println("✗ Failed to get time from API. HTTP Code: " + String(httpCode));
    
    // Fallback to NTP time server
    Serial.println("Trying NTP fallback...");
    syncRTCWithNTP();
  }
  
  http.end();
}

void syncRTCWithNTP() {
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // GMT+7 for Jakarta
  
  Serial.print("Waiting for NTP time sync");
  int attempts = 0;
  while (time(nullptr) < 1000000000 && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  time_t now = time(nullptr);
  if (now > 1000000000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    rtc.adjust(ntpTime);
    rtcSynced = true;
    
    Serial.println("✓ RTC Synchronized with NTP");
    Serial.printf("RTC Updated Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  ntpTime.year(), ntpTime.month(), ntpTime.day(),
                  ntpTime.hour(), ntpTime.minute(), ntpTime.second());
    
  } else {
    Serial.println("✗ NTP time sync failed");    
  }
}

void createCSVHeaders() {
  if (!sdCardAvailable) return;
  
  // Create attendance.csv header
  if (!SD.exists("/attendance.csv")) {
    File file = SD.open("/attendance.csv", FILE_WRITE);
    if (file) {
      file.println("Timestamp,CardUID,DeviceID,Status,Synced,Date,Time,RowID,PhotoURL,PhotoStatus");
      file.close();
      Serial.println("✓ attendance.csv created with header");
    }
  }
  
  // Create backup.csv header
  if (!SD.exists("/backup.csv")) {
    File file = SD.open("/backup.csv", FILE_WRITE);
    if (file) {
      file.println("Timestamp,CardUID,DeviceID,Status,Synced,Date,Time,RowID");
      file.close();
      Serial.println("✓ backup.csv created with header");
    }
  }
}

void processBackupDataAtStartup() {
  Serial.println("\n=== STARTING BACKUP SYNC ===");  
  
  if (!SD.exists("/backup.csv")) {
    Serial.println("No backup file found - skipping backup sync");
    backupProcessed = true;
    return;
  }
  
  File file = SD.open("/backup.csv", FILE_READ);
  if (!file) {
    Serial.println("Cannot open backup file");
    backupProcessed = true;
    return;
  }
  
  // Skip header line
  file.readStringUntil('\n');
  
  // Hitung jumlah data backup
  int backupCount = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      backupCount++;
    }
  }
  file.close();
  
  if (backupCount == 0) {
    Serial.println("No backup data to sync");
    backupProcessed = true;
    return;
  }
  
  Serial.println("Found " + String(backupCount) + " backup entries to sync");
  
  // Baca ulang file untuk proses sync
  file = SD.open("/backup.csv", FILE_READ);
  file.readStringUntil('\n'); // Skip header
  
  String backupData[50];
  int actualCount = 0;
  
  while (file.available() && actualCount < 50) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      backupData[actualCount] = line;
      actualCount++;
    }
  }
  file.close();
  
  // Process backup data
  int successCount = 0;
  for (int i = 0; i < actualCount; i++) {
    String csvLine = backupData[i];
    
    // Convert CSV to JSON (TANPA FOTO)
    String jsonData = convertCSVToJSON(csvLine);
    String rowId = sendToGoogleAppsScript(jsonData);
    
    if (rowId != "") {
      Serial.println("✓ Backup sync: " + csvLine.substring(0, 30) + "...");
      successCount++;
      backupData[i] = ""; // Mark as processed
      
      // Update attendance.csv dengan RowID
      updateAttendanceWithRowID(csvLine, rowId);
    } else {
      Serial.println("✗ Failed to sync: " + csvLine.substring(0, 30));
    }
    
    delay(800); // Delay antara requests untuk tidak overload server
  }
  
  // Tulis ulang backup file tanpa data yang berhasil disinkronisasi
  if (successCount > 0) {
    rewriteBackupFile(backupData, actualCount);
    Serial.println("✓ Backup file updated: " + String(successCount) + " entries synced and removed");
  }
  
  Serial.println("★ BACKUP SYNC COMPLETE: " + String(successCount) + "/" + String(actualCount) + " entries synced");
  backupProcessed = true;
  Serial.println("=== BACKUP SYNC FINISHED ===\n");
}

void rewriteBackupFile(String backupData[], int count) {
  SD.remove("/backup.csv");
  
  File file = SD.open("/backup.csv", FILE_WRITE);
  if (!file) return;
  
  // Write header
  file.println("Timestamp,CardUID,DeviceID,Status,Synced,Date,Time,RowID");
  
  // Write remaining data
  for (int i = 0; i < count; i++) {
    if (backupData[i] != "") {
      file.println(backupData[i]);
    }
  }
  
  file.close();
}

String createCSVData(String uid, String timestamp, String date, String time, String rowId, String photoStatus) {
  String csvLine = "\"" + timestamp + "\",";     // Timestamp
  csvLine += "\"" + uid + "\",";                 // CardUID
  csvLine += "\"ESP32_ATTENDANCE_01\",";         // DeviceID
  csvLine += "\"" + String(isOnline ? "ONLINE" : "OFFLINE") + "\","; // Status
  csvLine += "\"" + String(rtcSynced ? "SYNCED" : "NOT_SYNCED") + "\","; // Synced
  csvLine += "\"" + date + "\",";                // Date
  csvLine += "\"" + time + "\",";                // Time
  csvLine += "\"" + rowId + "\",";               // RowID
  csvLine += "\"" + photoStatus + "\"";          // PhotoStatus
  
  return csvLine;
}

String createJSONData(String uid, String timestamp, String date, String time) {
  DynamicJsonDocument doc(512);
  doc["device_id"] = "ESP32_ATTENDANCE_01";
  doc["card_uid"] = uid;
  doc["timestamp"] = timestamp;
  doc["date"] = date;
  doc["time"] = time;
  doc["status"] = isOnline ? "online" : "offline";
  doc["rtc_synced"] = rtcSynced;
  doc["type"] = "attendance";
  doc["photo_status"] = "pending";
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

bool saveToCSV(String filename, String csvData) {
  digitalWrite(SS_RFID, HIGH);
  
  File file = SD.open(filename, FILE_APPEND);
  if (!file) {
    Serial.println("✗ Cannot open " + String(filename));
    digitalWrite(SS_SD, HIGH);
    return false;
  }
  
  file.println(csvData);
  file.close();
  
  digitalWrite(SS_SD, HIGH);
  return true;
}

String convertCSVToJSON(String csvLine) {
  // Parse CSV line - format: "Timestamp","CardUID","DeviceID","Status","Synced","Date","Time","RowID"
  Serial.println("Parsing CSV: " + csvLine);
  
  // Cari semua posisi koma (untuk 8 field)
  int fieldPositions[8];
  int fieldCount = 0;
  bool inQuotes = false;
  
  for (int i = 0; i < csvLine.length() && fieldCount < 8; i++) {
    if (csvLine.charAt(i) == '\"') {
      inQuotes = !inQuotes; // Toggle quote state
    } else if (csvLine.charAt(i) == ',' && !inQuotes) {
      fieldPositions[fieldCount] = i;
      fieldCount++;
    }
  }
  
  // Pastikan kita punya cukup field
  if (fieldCount < 7) {
    Serial.println("Error: Invalid CSV format, expected 8 fields, got " + String(fieldCount + 1));
    return "{}";
  }
  
  // Extract fields (perhatikan format dengan quotes)
  String timestamp = extractCSVField(csvLine, 0, fieldPositions[0]);
  String uid = extractCSVField(csvLine, fieldPositions[0] + 1, fieldPositions[1]);
  String deviceId = extractCSVField(csvLine, fieldPositions[1] + 1, fieldPositions[2]);
  String status = extractCSVField(csvLine, fieldPositions[2] + 1, fieldPositions[3]);
  String synced = extractCSVField(csvLine, fieldPositions[3] + 1, fieldPositions[4]);
  String date = extractCSVField(csvLine, fieldPositions[4] + 1, fieldPositions[5]);
  String time = extractCSVField(csvLine, fieldPositions[5] + 1, fieldPositions[6]);
  String rowId = extractCSVField(csvLine, fieldPositions[6] + 1, csvLine.length());
  
  // Debug output
  Serial.println("Parsed fields:");
  Serial.println("  Timestamp: " + timestamp);
  Serial.println("  UID: " + uid);
  Serial.println("  Date: " + date);
  Serial.println("  Time: " + time);
  Serial.println("  Status: " + status);
  Serial.println("  Synced: " + synced);
  
  DynamicJsonDocument doc(512);
  doc["device_id"] = "ESP32_ATTENDANCE_01";
  doc["card_uid"] = uid;
  doc["timestamp"] = timestamp;
  doc["date"] = date;
  doc["time"] = time;
  doc["status"] = "online";
  doc["rtc_synced"] = (synced == "\"SYNCED\"" || synced == "SYNCED");
  doc["type"] = "backup_attendance";
  doc["photo_status"] = "no_photo";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("Generated JSON: " + jsonString);
  return jsonString;
}

// Helper function untuk extract field dari CSV dengan quotes
String extractCSVField(String csvLine, int start, int end) {
  String field = csvLine.substring(start, end);
  field.trim();
  
  // Remove surrounding quotes jika ada
  if (field.length() >= 2 && field.charAt(0) == '\"' && field.charAt(field.length() - 1) == '\"') {
    field = field.substring(1, field.length() - 1);
  }
  
  return field;
}

String formatTimestamp(DateTime dt) {
  char timestamp[20];
  sprintf(timestamp, "%04d-%02d-%02d %02d:%02d:%02d", 
          dt.year(), dt.month(), dt.day(), 
          dt.hour(), dt.minute(), dt.second());
  return String(timestamp);
}

String formatDate(DateTime dt) {
  char date[11];
  sprintf(date, "%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
  return String(date);
}

String formatTime(DateTime dt) {
  char time[9];
  sprintf(time, "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
  return String(time);
}

String getRFIDUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void updateAttendanceWithRowID(String csvLine, String rowId) {
  // Parse CSV backup line untuk mendapatkan timestamp dan UID
  int firstComma = csvLine.indexOf(',');
  int secondComma = csvLine.indexOf(',', firstComma + 1);
  
  String timestamp = csvLine.substring(1, firstComma - 1);
  String uid = csvLine.substring(firstComma + 2, secondComma - 1);
  
  // Cari dan update entry di attendance.csv
  if (!SD.exists("/attendance.csv")) return;
  
  File file = SD.open("/attendance.csv", FILE_READ);
  if (!file) return;
  
  // Baca semua data attendance
  String attendanceData[100];
  int attendanceCount = 0;
  
  // Header
  attendanceData[attendanceCount] = file.readStringUntil('\n');
  attendanceCount++;
  
  while (file.available() && attendanceCount < 100) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      attendanceData[attendanceCount] = line;
      attendanceCount++;
    }
  }
  file.close();
  
  // Cari entry yang sesuai dan update RowID
  bool updated = false;
  for (int i = 1; i < attendanceCount; i++) {
    String line = attendanceData[i];
    if (line.indexOf(timestamp) != -1 && line.indexOf(uid) != -1) {
      // Update line dengan RowID
      int lastComma = line.lastIndexOf(',');
      if (lastComma != -1) {
        String newLine = line.substring(0, lastComma + 1) + "\"" + rowId + "\"";
        attendanceData[i] = newLine;
        updated = true;
        Serial.println("✓ Updated attendance.csv with RowID: " + rowId);
        break;
      }
    }
  }
  
  // Tulis ulang attendance.csv jika ada perubahan
  if (updated) {
    SD.remove("/attendance.csv");
    File newFile = SD.open("/attendance.csv", FILE_WRITE);
    if (newFile) {
      for (int i = 0; i < attendanceCount; i++) {
        newFile.println(attendanceData[i]);
      }
      newFile.close();
    }
  }
}

void printSystemStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.println("WiFi: " + String(isOnline ? "Connected" : "Disconnected"));
  Serial.println("SD Card: " + String(sdCardAvailable ? "Available" : "Not Available"));
  Serial.println("RTC: " + String(rtc.begin() ? "Available" : "Not Available"));
  Serial.println("RTC Synced: " + String(rtcSynced ? "Yes" : "No"));
  Serial.println("Backup Processed: " + String(backupProcessed ? "Yes" : "No"));
  Serial.println("System Ready: " + String(systemReady ? "Yes" : "No"));
  Serial.println("RFID: Available");
  
  if (rtcSynced) {
    DateTime now = rtc.now();
    Serial.printf("Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());
  }
  Serial.println("==================\n");
}