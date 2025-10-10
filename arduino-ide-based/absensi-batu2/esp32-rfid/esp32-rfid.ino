#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <RTClib.h>
#include <ArduinoJson.h>

// Shared SPI Pins
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

// Chip Select Pins
#define SS_RFID 4
#define SS_SD 13
#define RST_RFID 5

// LED Onboard Pin (biasanya GPIO 2 pada ESP32 DevKit)
#define LED_ONBOARD 2

MFRC522 mfrc522(SS_RFID, RST_RFID);
RTC_DS3231 rtc;

const char* ESP32CAM_URL = "http://esp32cam.local/api/attendance";
const char* TIME_API_URL = "http://worldtimeapi.org/api/timezone/Asia/Makassar";

// WiFi credentials
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

bool isOnline = false;
bool sdCardAvailable = false;
bool rtcSynced = false;

// LED control variables
unsigned long lastLedBlink = 0;
bool ledState = false;
int ledMode = 0; // 0=off, 1=slow blink, 2=fast blink, 3=on, 4=processing blink

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32 Attendance System...");
  
  // Initialize LED Onboard
  pinMode(LED_ONBOARD, OUTPUT);
  digitalWrite(LED_ONBOARD, LOW);
  
  // Startup LED sequence
  ledSequence(3, 200); // 3x blink cepat saat startup
  
  // Initialize Shared SPI Bus
  initSPI();
  
  // Initialize devices
  initRFID();
  initSDCard();
  initRTC();
  initWiFi();
  
  // Synchronize RTC with internet time
  if (isOnline) {
    setLedMode(2); // Fast blink selama sync RTC
    syncRTCTime();
  } else {
    Serial.println("⚠ Cannot sync RTC - No internet connection");
  }
  
  // Create CSV header if file doesn't exist
  createCSVHeader();
  
  // Set LED to standby mode
  setLedMode(1); // Slow blink standby
  
  Serial.println("System Ready - Tap RFID Card to Begin");
  printSystemStatus();
}

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
    return;
  }
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("✗ No SD Card detected");
    sdCardAvailable = false;
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
  uint64_t usedSpace = SD.usedBytes() / (1024 * 1024);
  Serial.printf("✓ SD Card Size: %lluMB\n", cardSize);
  Serial.printf("✓ Used Space: %lluMB\n", usedSpace);
  
  sdCardAvailable = true;
  Serial.println("✓ SD Card Initialized Successfully");
}

void initRTC() {
  if (!rtc.begin()) {
    Serial.println("✗ RTC not found!");
    return;
  }
  
  Serial.println("✓ RTC Found");
  
  // Display current RTC time
  DateTime now = rtc.now();
  Serial.printf("RTC Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
}

void initWiFi() {
  Serial.print("Connecting to WiFi");
  setLedMode(2); // Fast blink selama connecting WiFi
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
    updateLED(); // Update LED selama connecting
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    isOnline = true;
    Serial.println("\n✓ WiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // LED indication for WiFi connected
    ledSequence(2, 300); // 2x blink untuk WiFi connected
  } else {
    isOnline = false;
    Serial.println("\n✗ WiFi Failed - Operating in Offline Mode");
    
    // LED indication for WiFi failed
    ledSequence(4, 150); // 4x blink cepat untuk WiFi failed
  }
}

void syncRTCTime() {
  if (!isOnline) {
    Serial.println("⚠ Cannot sync RTC - No internet connection");
    return;
  }
  
  Serial.println("Synchronizing RTC with internet time...");
  setLedMode(2); // Fast blink selama sync
  
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
      
      // LED success indication
      ledSequence(1, 500); // 1x blink panjang untuk success
    } else {
      Serial.println("✗ Failed to parse time API response");
      ledSequence(3, 200); // 3x blink untuk error
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
    updateLED(); // Update LED selama sync NTP
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
    
    // LED success indication
    ledSequence(2, 300); // 2x blink untuk NTP success
  } else {
    Serial.println("✗ NTP time sync failed");
    ledSequence(4, 150); // 4x blink cepat untuk NTP failed
  }
}

void createCSVHeader() {
  if (!sdCardAvailable) return;
  
  // Check if file exists, if not create with header
  if (!SD.exists("/attendance.csv")) {
    digitalWrite(SS_RFID, HIGH);
    File file = SD.open("/attendance.csv", FILE_WRITE);
    
    if (file) {
      // CSV Header
      file.println("Timestamp,CardUID,DeviceID,Status,Synced,Date,Time");
      file.close();
      Serial.println("✓ CSV file created with header");
    } else {
      Serial.println("✗ Failed to create CSV file");
    }
    
    digitalWrite(SS_SD, HIGH);
  } else {
    Serial.println("✓ CSV file already exists");
  }
}

void loop() {
  // Update LED status
  updateLED();
  
  // Check for RFID card
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    processRFIDCard();
    mfrc522.PICC_HaltA();
    delay(1000);
  }
  
  // Periodic RTC sync (every 1 hour)
  static unsigned long lastSync = 0;
  if (isOnline && millis() - lastSync > 3600000) {
    lastSync = millis();
    Serial.println("Performing periodic RTC sync...");
    syncRTCTime();
  }
  
  // Process pending data if online (every 30 seconds)
  static unsigned long lastProcess = 0;
  if (isOnline && millis() - lastProcess > 30000) {
    lastProcess = millis();
    processPendingData();
  }
  
  delay(100);
}

// ==================== LED CONTROL FUNCTIONS ====================

void setLedMode(int mode) {
  ledMode = mode;
  lastLedBlink = millis();
  ledState = false;
  digitalWrite(LED_ONBOARD, LOW); // Reset ke OFF
}

void updateLED() {
  unsigned long currentMillis = millis();
  
  switch(ledMode) {
    case 0: // LED OFF
      digitalWrite(LED_ONBOARD, LOW);
      break;
      
    case 1: // Slow Blink (Standby - menunggu tap kartu)
      if (currentMillis - lastLedBlink > 2000) { // Blink setiap 2 detik
        lastLedBlink = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_ONBOARD, ledState);
      }
      break;
      
    case 2: // Fast Blink (Processing/Connecting)
      if (currentMillis - lastLedBlink > 300) { // Blink setiap 300ms
        lastLedBlink = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_ONBOARD, ledState);
      }
      break;
      
    case 3: // LED ON
      digitalWrite(LED_ONBOARD, HIGH);
      break;
      
    case 4: // Processing Blink (sedang memproses data)
      if (currentMillis - lastLedBlink > 100) { // Blink sangat cepat setiap 100ms
        lastLedBlink = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_ONBOARD, ledState);
      }
      break;
  }
}

void ledSequence(int blinks, int duration) {
  // Override current mode untuk sequence khusus
  int previousMode = ledMode;
  setLedMode(0); // Matikan LED sementara
  
  for(int i = 0; i < blinks; i++) {
    digitalWrite(LED_ONBOARD, HIGH);
    delay(duration);
    digitalWrite(LED_ONBOARD, LOW);
    if(i < blinks - 1) delay(duration);
  }
  
  // Kembali ke mode sebelumnya
  setLedMode(previousMode);
}

// ==================== MAIN PROCESSING FUNCTIONS ====================

void processRFIDCard() {
  Serial.println("\n=== RFID Card Detected ===");
  setLedMode(4); // Processing blink - sangat cepat
  
  String uid = getRFIDUID();
  DateTime now = rtc.now();
  String timestamp = formatTimestamp(now);
  String date = formatDate(now);
  String time = formatTime(now);
  
  Serial.println("UID: " + uid);
  Serial.println("Date: " + date);
  Serial.println("Time: " + time);
  Serial.println("RTC Synced: " + String(rtcSynced ? "Yes" : "No"));
  
  // Create CSV data
  String csvData = createCSVData(uid, timestamp, date, time);
  
  // Save to SD card as CSV
  if (sdCardAvailable) {
    if (saveToCSV(csvData)) {
      Serial.println("✓ Data saved to CSV");
      ledSequence(1, 200); // 1x blink untuk save success
    } else {
      Serial.println("✗ Failed to save to CSV");
      ledSequence(3, 150); // 3x blink untuk save failed
    }
  } else {
    Serial.println("⚠ SD Card not available - data not saved locally");
    ledSequence(2, 200); // 2x blink untuk SD not available
  }
  
  // Create JSON for ESP32-CAM
  String jsonData = createJSONData(uid, timestamp, date, time);
  
  // Send to ESP32-CAM if online
  if (isOnline) {
    setLedMode(2); // Fast blink selama mengirim data
    if (sendToESP32CAM(jsonData)) {
      Serial.println("✓ Data sent to ESP32-CAM");
      ledSequence(2, 300); // 2x blink untuk send success
    } else {
      Serial.println("✗ Failed to send to ESP32-CAM");
      ledSequence(4, 150); // 4x blink cepat untuk send failed
    }
  } else {
    Serial.println("⚠ Offline - data not sent to ESP32-CAM");
    ledSequence(1, 500); // 1x blink panjang untuk offline
  }
  
  // Kembali ke standby mode
  setLedMode(1); // Slow blink standby
  
  Serial.println("=== Processing Complete ===\n");
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

String createCSVData(String uid, String timestamp, String date, String time) {
  String csvLine = "\"" + timestamp + "\",";     // Timestamp
  csvLine += "\"" + uid + "\",";                 // CardUID
  csvLine += "\"ESP32_ATTENDANCE_01\",";         // DeviceID
  csvLine += "\"" + String(isOnline ? "ONLINE" : "OFFLINE") + "\","; // Status
  csvLine += "\"" + String(rtcSynced ? "SYNCED" : "NOT_SYNCED") + "\","; // Synced
  csvLine += "\"" + date + "\",";                // Date
  csvLine += "\"" + time + "\"";                 // Time
  
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
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

bool saveToCSV(String csvData) {
  digitalWrite(SS_RFID, HIGH);
  
  File file = SD.open("/attendance.csv", FILE_APPEND);
  if (!file) {
    Serial.println("✗ Cannot open CSV file");
    digitalWrite(SS_SD, HIGH);
    return false;
  }
  
  file.println(csvData);
  file.close();
  
  digitalWrite(SS_SD, HIGH);
  return true;
}

bool sendToESP32CAM(String jsonData) {
  HTTPClient http;
  http.begin(ESP32CAM_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);
  
  int httpCode = http.POST(jsonData);
  bool success = (httpCode == 200);
  
  if (success) {
    String response = http.getString();
    Serial.println("ESP32-CAM Response: " + response);
  } else {
    Serial.println("HTTP Error: " + String(httpCode));
  }
  
  http.end();
  return success;
}

void processPendingData() {
  if (!sdCardAvailable) return;
  
  Serial.println("Checking for pending data to send...");
  setLedMode(2); // Fast blink selama processing pending data
  
  digitalWrite(SS_RFID, HIGH);
  
  File file = SD.open("/attendance.csv", FILE_READ);
  if (!file) {
    Serial.println("No CSV file found");
    digitalWrite(SS_SD, HIGH);
    setLedMode(1); // Kembali ke standby
    return;
  }
  
  // Skip header line
  file.readStringUntil('\n');
  
  int pendingCount = 0;
  int sentCount = 0;
  
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    
    if (line.length() > 0) {
      pendingCount++;
      
      // Convert CSV line to JSON for sending
      String jsonData = convertCSVToJSON(line);
      if (sendToESP32CAM(jsonData)) {
        sentCount++;
        Serial.println("✓ Sent pending data: " + line.substring(0, 30) + "...");
      }
    }
  }
  
  file.close();
  digitalWrite(SS_SD, HIGH);
  
  Serial.printf("Pending data processed: %d sent out of %d total\n", sentCount, pendingCount);
  
  // Kembali ke standby mode
  setLedMode(1); // Slow blink standby
}

String convertCSVToJSON(String csvLine) {
  // Simple CSV to JSON conversion
  int firstComma = csvLine.indexOf(',');
  int secondComma = csvLine.indexOf(',', firstComma + 1);
  
  String timestamp = csvLine.substring(1, firstComma - 1); // Remove quotes
  String uid = csvLine.substring(firstComma + 2, secondComma - 1); // Remove quotes
  
  DynamicJsonDocument doc(512);
  doc["device_id"] = "ESP32_ATTENDANCE_01";
  doc["card_uid"] = uid;
  doc["timestamp"] = timestamp;
  doc["type"] = "pending_attendance";
  doc["status"] = "offline_data";
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

void printSystemStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.println("WiFi: " + String(isOnline ? "Connected" : "Disconnected"));
  Serial.println("SD Card: " + String(sdCardAvailable ? "Available" : "Not Available"));
  Serial.println("RTC: " + String(rtc.begin() ? "Available" : "Not Available"));
  Serial.println("RTC Synced: " + String(rtcSynced ? "Yes" : "No"));
  Serial.println("RFID: Available");
  Serial.println("LED Mode: " + String(ledMode));
  
  if (rtcSynced) {
    DateTime now = rtc.now();
    Serial.printf("Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());
  }
  Serial.println("==================\n");
}