#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#include <SD.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// Pin definitions
#define SS_PIN_RFID 4
#define RST_PIN_RFID 5
#define SS_PIN_SD 13

// WiFi Credentials
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// Google Sheets API Configuration
const char* spreadsheetId = "1PrE6lcD2UlXVGeDsl2ADjQpauOwGioSNTBOsdHIx35Q";
const char* sheetName = "Data Siswa";
const char* apiKey = "AIzaSyALePRtFy01Dr3QzhKxroL7dmOhpKPnBa8"; // Dapatkan dari Google Cloud Console

// Google Apps Script Web App URL (untuk absensi)
const char* webAppURL = "https://script.google.com/macros/s/AKfycbz1e6MWnfqxIWLpKyPuW51ZmF64kTkDZk_1vyG7ui3Y8jMfj6c96L70XUhgKrwzuHI5/exec";

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
String currentFotoID = "";

// Struct untuk data siswa
struct Siswa {
  String nis;
  String kelas;
  String nama;
  String uid;
};

const int MAX_SISWA = 100;
Siswa dataSiswa[MAX_SISWA];
int jumlahSiswa = 0;

// WiFi Client Secure untuk HTTPS
WiFiClientSecure client;

// Fungsi untuk generate random string
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

// Fungsi untuk extract URL dari HTML redirect
String extractRedirectURL(String htmlContent) {
  // Cari tag <A HREF=
  int hrefIndex = htmlContent.indexOf("HREF=\"");
  if (hrefIndex == -1) {
    hrefIndex = htmlContent.indexOf("href=\"");
  }
  
  if (hrefIndex != -1) {
    int start = hrefIndex + 6; // Panjang "HREF="" atau "href=""
    int end = htmlContent.indexOf("\"", start);
    
    if (end != -1) {
      return htmlContent.substring(start, end);
    }
  }
  
  return "";
}

// Fungsi untuk HTTP request dengan handle redirect
String httpRequestWithRedirect(const String& url, const String& payload = "", bool isPost = false) {
  HTTPClient http;
  String finalURL = url;
  String response = "";
  int maxRedirects = 3;
  int redirectCount = 0;
  
  while (redirectCount < maxRedirects) {
    Serial.print("HTTP Request to: ");
    Serial.println(finalURL);
    
    http.begin(finalURL);
    
    if (isPost) {
      http.addHeader("Content-Type", "application/json");
    }
    
    int httpResponseCode;
    if (isPost) {
      httpResponseCode = http.POST(payload);
    } else {
      httpResponseCode = http.GET();
    }
    
    if (httpResponseCode > 0) {
      response = http.getString();
      
      // Cek jika response adalah HTML redirect
      if (response.indexOf("Moved Temporarily") != -1 || 
          response.indexOf("The document has moved") != -1) {
        
        String newURL = extractRedirectURL(response);
        if (newURL != "") {
          Serial.print("Redirect detected to: ");
          Serial.println(newURL);
          finalURL = newURL;
          redirectCount++;
          http.end();
          continue; // Coba lagi dengan URL baru
        }
      }
      
      // Jika bukan redirect, return response
      http.end();
      return response;
      
    } else {
      Serial.print("Error in HTTP request: ");
      Serial.println(httpResponseCode);
      http.end();
      return "";
    }
  }
  
  Serial.println("Max redirects reached");
  return "";
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
  
  // Set root CA untuk HTTPS
  client.setInsecure(); // Hati-hati: ini tidak verify certificate
  
  // Connect to WiFi
  connectToWiFi();
  
  // Sinkronisasi data siswa dari Google Sheets
  if (WiFi.status() == WL_CONNECTED) {
    if (!syncDataSiswaSheetsAPI()) {
      // Jika Sheets API gagal, coba metode alternatif
      syncDataSiswaAlternative();
    }
  } else {
    bacaDataSiswaDariSD();
  }
  
  // Upload data backup
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
  Serial.print("Jumlah siswa terdaftar: ");
  Serial.println(jumlahSiswa);
}

// Method 1: Google Sheets API v4
bool syncDataSiswaSheetsAPI() {
  Serial.println("Menggunakan Google Sheets API...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sheets API...");
  
  String url = "https://sheets.googleapis.com/v4/spreadsheets/";
  url += spreadsheetId;
  url += "/values/";
  url += sheetName;
  url += "?key=";
  url += apiKey;
  
  HTTPClient http;
  http.begin(client, url);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Data received from Sheets API");
    
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      JsonArray values = doc["values"];
      jumlahSiswa = 0;
      
      // Skip header row (index 0)
      for (int i = 1; i < values.size() && jumlahSiswa < MAX_SISWA; i++) {
        JsonArray row = values[i];
        if (row.size() >= 4) {
          dataSiswa[jumlahSiswa].nis = row[0].as<String>();
          dataSiswa[jumlahSiswa].kelas = row[1].as<String>();
          dataSiswa[jumlahSiswa].nama = row[2].as<String>();
          dataSiswa[jumlahSiswa].uid = row[3].as<String>();
          jumlahSiswa++;
        }
      }
      
      simpanDataSiswaKeSD();
      
      Serial.print("Sheets API Success: ");
      Serial.println(jumlahSiswa);
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("API Success!");
      lcd.setCursor(0, 1);
      lcd.print("Siswa: ");
      lcd.print(jumlahSiswa);
      
      http.end();
      return true;
    }
  }
  
  Serial.println("Sheets API failed");
  http.end();
  return false;
}

// Method 2: Alternative - Simpan data langsung di kode
void syncDataSiswaAlternative() {
  Serial.println("Menggunakan metode alternatif...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Alternative");
  lcd.setCursor(0, 1);
  lcd.print("Method...");
  
  // Coba baca dari SD card dulu
  bacaDataSiswaDariSD();
  
  // Jika tidak ada data di SD, gunakan data default
  if (jumlahSiswa == 0) {
    // Tambahkan beberapa data contoh
    // Ganti dengan data siswa sebenarnya
    dataSiswa[0] = {"12345", "XII IPA 1", "John Doe", "A1B2C3D4"};
    dataSiswa[1] = {"12346", "XII IPA 1", "Jane Smith", "E5F6G7H8"};
    jumlahSiswa = 2;
    
    simpanDataSiswaKeSD();
    
    Serial.println("Using default data");
  }
  
  delay(2000);
}

// Method 3: Simple HTTP dengan handle redirect manual
bool syncDataSiswaSimpleHTTP() {
  Serial.println("Menggunakan Simple HTTP...");
  
  String url = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec?action=getSiswa";
  
  HTTPClient http;
  http.begin(client, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Cek jika ini HTML redirect
    if (payload.indexOf("Moved Temporarily") != -1) {
      Serial.println("Redirect detected, trying to extract URL...");
      
      // Extract redirect URL
      int hrefStart = payload.indexOf("HREF=\"");
      if (hrefStart == -1) hrefStart = payload.indexOf("href=\"");
      
      if (hrefStart != -1) {
        int urlStart = hrefStart + 6;
        int urlEnd = payload.indexOf("\"", urlStart);
        String newUrl = payload.substring(urlStart, urlEnd);
        
        Serial.print("New URL: ");
        Serial.println(newUrl);
        
        // Coba dengan URL baru
        http.end();
        http.begin(client, newUrl);
        httpCode = http.GET();
        
        if (httpCode == HTTP_CODE_OK) {
          payload = http.getString();
        }
      }
    }
    
    // Parse JSON
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      JsonArray data = doc["data"];
      jumlahSiswa = 0;
      
      for (JsonObject obj : data) {
        if (jumlahSiswa < MAX_SISWA) {
          dataSiswa[jumlahSiswa].nis = obj["nis"].as<String>();
          dataSiswa[jumlahSiswa].kelas = obj["kelas"].as<String>();
          dataSiswa[jumlahSiswa].nama = obj["nama"].as<String>();
          dataSiswa[jumlahSiswa].uid = obj["uid"].as<String>();
          jumlahSiswa++;
        }
      }
      
      simpanDataSiswaKeSD();
      http.end();
      return true;
    }
  }
  
  http.end();
  return false;
}

// [Fungsi-fungsi lainnya tetap sama...]
void simpanDataSiswaKeSD() {
  dataFile = SD.open("/data_siswa.csv", FILE_WRITE);
  
  if (dataFile) {
    dataFile.println("NIS,Kelas,Nama,UID");
    
    for (int i = 0; i < jumlahSiswa; i++) {
      dataFile.print(dataSiswa[i].nis);
      dataFile.print(",");
      dataFile.print(dataSiswa[i].kelas);
      dataFile.print(",");
      dataFile.print(dataSiswa[i].nama);
      dataFile.print(",");
      dataSiswa[i].uid.toUpperCase();
      dataFile.println(dataSiswa[i].uid);
    }
    
    dataFile.close();
    Serial.println("Data siswa disimpan ke SD card");
  }
}

void bacaDataSiswaDariSD() {
  if (!SD.exists("/data_siswa.csv")) return;
  
  dataFile = SD.open("/data_siswa.csv", FILE_READ);
  if (!dataFile) return;
  
  // Skip header
  dataFile.readStringUntil('\n');
  
  jumlahSiswa = 0;
  
  while (dataFile.available() && jumlahSiswa < MAX_SISWA) {
    String line = dataFile.readStringUntil('\n');
    line.trim();
    
    if (line.length() == 0) continue;
    
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    int thirdComma = line.indexOf(',', secondComma + 1);
    
    if (firstComma == -1 || secondComma == -1 || thirdComma == -1) continue;
    
    dataSiswa[jumlahSiswa].nis = line.substring(0, firstComma);
    dataSiswa[jumlahSiswa].kelas = line.substring(firstComma + 1, secondComma);
    dataSiswa[jumlahSiswa].nama = line.substring(secondComma + 1, thirdComma);
    dataSiswa[jumlahSiswa].uid = line.substring(thirdComma + 1);
    
    jumlahSiswa++;
  }
  
  dataFile.close();
  Serial.print("Data dari SD: ");
  Serial.println(jumlahSiswa);
}

Siswa* cariSiswaByUID(String uid) {
  for (int i = 0; i < jumlahSiswa; i++) {
    if (dataSiswa[i].uid.equalsIgnoreCase(uid)) {
      return &dataSiswa[i];
    }
  }
  return NULL;
}

void tampilkanInfoSiswa(Siswa* siswa) {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  String namaDisplay = siswa->nama;
  if (namaDisplay.length() > 16) {
    namaDisplay = namaDisplay.substring(0, 16);
  }
  lcd.print(namaDisplay);
  
  lcd.setCursor(0, 1);
  String infoDisplay = siswa->nis + " " + siswa->kelas;
  if (infoDisplay.length() > 16) {
    infoDisplay = infoDisplay.substring(0, 16);
  }
  lcd.print(infoDisplay);
}

void loop() {
  // [Kode loop tetap sama]
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
  
  // Cek apakah UID terdaftar dalam data siswa
  Siswa* siswa = cariSiswaByUID(cardUID);
  
  if (siswa != NULL) {
    // Jika terdaftar, tampilkan informasi siswa
    tampilkanInfoSiswa(siswa);
  } else {
    // Jika tidak terdaftar, tampilkan UID saja
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("UID: ");
    lcd.print(cardUID.substring(0, 11));
    lcd.setCursor(0, 1);
    lcd.print(cardUID.substring(11));
    lcd.print(" Tdk Daftar");
  }
  
  // Save to cloud and SD card
  processAttendance(cardUID);
  
  mfrc522.PICC_HaltA();
}

// Fungsi untuk sinkronisasi data siswa dari Google Sheets (DIMODIFIKASI)
void syncDataSiswa() {
  Serial.println("Memulai sinkronisasi data siswa...");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sync Data");
  lcd.setCursor(0, 1);
  lcd.print("Siswa...");
  
  String syncURL = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec?action=getSiswa";
  
  String response = httpRequestWithRedirect(syncURL);
  
  if (response != "") {
    Serial.println("Data siswa diterima:");
    Serial.println(response);
    
    // Parse JSON response
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      jumlahSiswa = 0;
      
      // Simpan ke array
      JsonArray data = doc["data"].as<JsonArray>();
      for (JsonObject obj : data) {
        if (jumlahSiswa < MAX_SISWA) {
          dataSiswa[jumlahSiswa].nis = obj["nis"].as<String>();
          dataSiswa[jumlahSiswa].kelas = obj["kelas"].as<String>();
          dataSiswa[jumlahSiswa].nama = obj["nama"].as<String>();
          dataSiswa[jumlahSiswa].uid = obj["uid"].as<String>();
          jumlahSiswa++;
        }
      }
      
      // Simpan ke SD card
      simpanDataSiswaKeSD();
      
      Serial.print("Sinkronisasi berhasil. Jumlah siswa: ");
      Serial.println(jumlahSiswa);
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sync Success!");
      lcd.setCursor(0, 1);
      lcd.print("Siswa: ");
      lcd.print(jumlahSiswa);
      
    } else {
      Serial.println("Error parsing JSON data siswa");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sync Error!");
      lcd.setCursor(0, 1);
      lcd.print("Parse Failed");
    }
  } else {
    Serial.println("Gagal mengambil data siswa dari server");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sync Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Server Error");
    
    // Coba baca dari SD card jika gagal sync
    bacaDataSiswaDariSD();
  }
  
  delay(2000);
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

// Fungsi untuk upload backup data (DIMODIFIKASI)
bool uploadBackupEntry(String tanggal, String waktu, String uid, String foto_id) {
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
  
  String response = httpRequestWithRedirect(webAppURL, jsonString, true);
  
  if (response != "") {
    // Parse response untuk cek status
    DynamicJsonDocument resDoc(1024);
    DeserializationError error = deserializeJson(resDoc, response);
    
    if (!error) {
      String status = resDoc["status"].as<String>();
      return status == "success";
    }
  }
  
  return false;
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

// Fungsi saveToCloud (DIMODIFIKASI)
bool saveToCloud(String uid) {
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
  
  String response = httpRequestWithRedirect(webAppURL, jsonString, true);
  
  if (response != "") {
    Serial.println("HTTP Response received");
    Serial.println("Response: " + response);
    
    // Parse response
    DynamicJsonDocument resDoc(1024);
    DeserializationError error = deserializeJson(resDoc, response);
    
    if (!error) {
      String status = resDoc["status"].as<String>();
      if (status == "success") {
        Serial.println("Data saved to cloud successfully");
        return true;
      }
    }
    
    Serial.println("Failed to save to cloud");
    return false;
  } else {
    Serial.println("Error in HTTP request");
    return false;
  }
}

// Fungsi untuk send foto ID ke camera (DIMODIFIKASI)
bool sendFotoIDToCamera() {
  String postData = "foto_id=" + currentFotoID + "&uid=" + lastCardUID;
  
  HTTPClient http;
  http.begin(esp32camURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
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