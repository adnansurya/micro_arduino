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

// Konfigurasi Hardware Serial untuk RS485
HardwareSerial modbusSerial(2);  // Gunakan UART2 ESP32
ModbusMaster node;

// Konfigurasi RTC DS3231
RTC_DS3231 rtc;

// Konfigurasi SD Card
const int chipSelect = 5;  // GPIO5 untuk CS SD card
File dataFile;

// Konfigurasi WiFi Manager
WiFiManager wm;

// URL Web App
const String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbw5HivqIgz4phUOFdWfPoQDfc_3VPm3T9pqjfDVJxdEw1ODb4Po_AO-k49P9BrhJah4nA/exec";

// API untuk mendapatkan waktu dari internet
const String TIME_API_URL = "http://worldtimeapi.org/api/timezone/Asia/Makassar";

// Alamat modbus device
static uint8_t modbusAddr = 1;

// Struktur untuk mendefinisikan register yang ingin dibaca
struct RegisterDefinition {
  uint16_t address;
  const char* name;
  const char* dataType;
  const char* jsonKey;
  float scaleFactor;
};

// Struktur untuk menyimpan hasil dalam format float
struct ReadingResult {
  const char* name;
  const char* jsonKey;
  float value;
};

// Daftar register yang ingin dibaca
RegisterDefinition registersToRead[] = {
  { 0x0042, "R phase Voltage", "U32", "voltageR", 10000.0 },
  { 0x0044, "S phase Voltage", "U32", "voltageS", 10000.0 },
  { 0x0046, "T phase Voltage", "U32", "voltageT", 10000.0 },
  { 0x0058, "R phase Current", "U32", "currentR", 10000.0 },
  { 0x005A, "S phase Current", "U32", "currentS", 10000.0 },
  { 0x005C, "T phase Current", "U32", "currentT", 10000.0 },
  { 0x0064, "R phase Active Power", "S32", "powerR", 10000.0 },
  { 0x0066, "S phase Active Power", "S32", "powerS", 10000.0 },
  { 0x0068, "T phase Active Power", "S32", "powerT", 10000.0 },
  { 0x006A, "Total Active Power", "S32", "totalActivePower", 10000.0 },
  { 0x006C, "R phase Reactive Power", "S32", "reactivePowerR", 10000.0 },
  { 0x006E, "S phase Reactive Power", "S32", "reactivePowerS", 10000.0 },
  { 0x0070, "T phase Reactive Power", "S32", "reactivePowerT", 10000.0 },
  { 0x0072, "Total Reactive Power", "S32", "totalReactivePower", 10000.0 },
  { 0x0074, "R phase Apparent Power", "S32", "apparentPowerR", 10000.0 },
  { 0x0076, "S phase Apparent Power", "S32", "apparentPowerS", 10000.0 },
  { 0x0078, "T phase Apparent Power", "S32", "apparentPowerT", 10000.0 },
  { 0x007A, "Total Apparent Power", "S32", "totalApparentPower", 10000.0 },
  { 0x007C, "R phase Power Factor", "S16", "powerFactorR", 1000.0 },
  { 0x007D, "S phase Power Factor", "S16", "powerFactorS", 1000.0 },
  { 0x007E, "T phase Power Factor", "S16", "powerFactorT", 1000.0 },
  { 0x007F, "Total Power Factor", "S16", "totalPowerFactor", 1000.0 },
  { 0x0080, "Frequency", "U16", "frequency", 100.0 },
};

const int numRegisters = sizeof(registersToRead) / sizeof(registersToRead[0]);

// Array untuk menyimpan hasil pembacaan dalam format float
ReadingResult readingResults[numRegisters];

// Variabel untuk manajemen data offline
bool sdCardAvailable = false;
bool rtcAvailable = false;
const String dataFileName = "/data_log.csv";
const String backupFileName = "/backup_data.csv";

// Variabel untuk WiFi Manager
bool shouldSaveConfig = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000; // 30 detik

// Variabel untuk time sync
bool timeSynced = false;
unsigned long lastTimeSync = 0;
const unsigned long TIME_SYNC_INTERVAL = 24 * 60 * 60 * 1000; // Sync setiap 24 jam

// Versi firmware
const String FIRMWARE_VERSION = "1.4.0-TIME-SYNC";

void setup() {
  modbusSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(115200);

  Serial.println();
  Serial.println("==================================");
  Serial.println("   ESP32 Data Logger dengan Time Sync");
  Serial.print("   Firmware Version: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("==================================");

  // Inisialisasi RTC
  if (rtc.begin()) {
    rtcAvailable = true;
    Serial.println("✅ RTC DS3231 terhubung");
    
    // Cek jika RTC kehilangan power
    if (rtc.lostPower()) {
      Serial.println("⚠️ RTC kehilangan power, perlu sync waktu dari internet");
    } else {
      Serial.println("✅ RTC waktu tersimpan");
      // Tampilkan waktu RTC saat ini
      DateTime now = rtc.now();
      Serial.print("🕒 Waktu RTC saat ini: ");
      Serial.println(getDateTimeString(now));
    }
  } else {
    Serial.println("❌ Gagal terhubung ke RTC DS3231");
  }

  // Inisialisasi SD Card
  if (SD.begin(chipSelect)) {
    sdCardAvailable = true;
    Serial.println("✅ SD Card terhubung");
    createCSVHeader();
  } else {
    Serial.println("❌ Gagal terhubung ke SD Card");
  }

  // Konfigurasi WiFi Manager
  setupWiFiManager();

  // Sync waktu dari internet setelah WiFi terkoneksi
  if (WiFi.status() == WL_CONNECTED && rtcAvailable) {
    syncTimeFromAPI();
  }

  Serial.println("Fort FCN300 Static Register Reader");
  Serial.println("Membaca register yang telah ditentukan (HEX)");
  Serial.println();

  // Tampilkan daftar register yang akan dibaca
  Serial.println("Register yang akan dibaca:");
  for (int i = 0; i < numRegisters; i++) {
    Serial.print("Reg 0x");
    Serial.print(registersToRead[i].address, HEX);
    Serial.print(" (");
    Serial.print(registersToRead[i].name);
    Serial.print("): ");
    Serial.print(registersToRead[i].dataType);
    Serial.print(", Skala: 1/");
    Serial.println(registersToRead[i].scaleFactor);
  }
  Serial.println();

  // Tampilkan info
  printSystemInfo();
}

void loop() {
  // Cek koneksi WiFi dan reconnect jika perlu
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastReconnectAttempt > RECONNECT_INTERVAL) {
      lastReconnectAttempt = currentMillis;
      Serial.println("⚠️ Koneksi WiFi terputus, mencoba reconnect...");
      WiFi.reconnect();
    }
  }

  // Sync waktu periodic setiap 24 jam
  if (WiFi.status() == WL_CONNECTED && rtcAvailable && !timeSynced) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastTimeSync > TIME_SYNC_INTERVAL) {
      syncTimeFromAPI();
    }
  }

  baca_registers_statistik();
  
  if (WiFi.status() == WL_CONNECTED) {
    sendOfflineData();
    kirim_data_ke_appscript();
  } else {
    Serial.println("⚠️ Koneksi WiFi terputus, menyimpan data ke SD Card...");
    simpan_data_ke_sd();
  }

  delay(30000);
}

// Fungsi untuk sync waktu dari API internet
bool syncTimeFromAPI() {
  if (!rtcAvailable) {
    Serial.println("❌ RTC tidak tersedia untuk sync waktu");
    return false;
  }

  Serial.println("🕒 Mensinkronisasi waktu dari internet...");

  HTTPClient http;
  WiFiClient client;

  // Gunakan HTTP (bukan HTTPS) untuk kemudahan
  if (!http.begin(client, TIME_API_URL)) {
    Serial.println("❌ Gagal terhubung ke time API");
    return false;
  }

  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("✅ Berhasil mendapatkan waktu dari API");
    
    // Parse JSON response
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("❌ Error parsing JSON: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }

    // Extract waktu dari JSON response
    // Format dari worldtimeapi.org: "2024-01-15T14:30:25.123456+07:00"
    String datetimeStr = doc["datetime"].as<String>();
    unsigned long unixtime = doc["unixtime"].as<unsigned long>();
    
    Serial.print("📅 Waktu dari API: ");
    Serial.println(datetimeStr);
    Serial.print("⏱️ Unix time: ");
    Serial.println(unixtime);

    // Konversi unixtime ke DateTime
    time_t rawtime = unixtime;
    struct tm *timeinfo = localtime(&rawtime);

    // Buat DateTime object
    DateTime apiTime(
      timeinfo->tm_year + 1900, // tahun sejak 1900
      timeinfo->tm_mon + 1,     // bulan 0-11 → 1-12
      timeinfo->tm_mday,        // hari
      timeinfo->tm_hour,        // jam
      timeinfo->tm_min,         // menit
      timeinfo->tm_sec          // detik
    );

    // Set RTC dengan waktu dari API
    rtc.adjust(apiTime);
    
    timeSynced = true;
    lastTimeSync = millis();
    
    Serial.print("✅ Waktu RTC disinkronisasi: ");
    Serial.println(getDateTimeString(apiTime));
    
    http.end();
    return true;
    
  } else {
    Serial.printf("❌ Gagal mendapatkan waktu, HTTP code: %d\n", httpCode);
    // Coba API alternatif
    return syncTimeFromBackupAPI();
  }
  
  http.end();
  return false;
}

// Fungsi backup menggunakan API alternatif
bool syncTimeFromBackupAPI() {
  Serial.println("🔄 Mencoba API waktu alternatif...");
  
  const String backupAPI = "http://worldtimeapi.org/api/ip"; // API alternatif
  
  HTTPClient http;
  WiFiClient client;

  if (!http.begin(client, backupAPI)) {
    Serial.println("❌ Gagal terhubung ke backup time API");
    return false;
  }

  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("❌ Error parsing JSON backup API: ");
      Serial.println(error.c_str());
      http.end();
      return false;
    }

    String datetimeStr = doc["datetime"].as<String>();
    unsigned long unixtime = doc["unixtime"].as<unsigned long>();
    
    Serial.print("📅 Waktu dari backup API: ");
    Serial.println(datetimeStr);

    time_t rawtime = unixtime;
    struct tm *timeinfo = localtime(&rawtime);

    DateTime apiTime(
      timeinfo->tm_year + 1900,
      timeinfo->tm_mon + 1,
      timeinfo->tm_mday,
      timeinfo->tm_hour,
      timeinfo->tm_min,
      timeinfo->tm_sec
    );

    rtc.adjust(apiTime);
    
    timeSynced = true;
    lastTimeSync = millis();
    
    Serial.print("✅ Waktu RTC disinkronisasi dari backup API: ");
    Serial.println(getDateTimeString(apiTime));
    
    http.end();
    return true;
  }
  
  http.end();
  return false;
}

void setupWiFiManager() {
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalTimeout(180);
  wm.setHostname("esp32-datalogger");

  Serial.println("Menyiapkan WiFi Manager...");
  
  if (!wm.autoConnect("ESP32-DataLogger", "password123")) {
    Serial.println("❌ Gagal terhubung dan timeout configuration portal");
    delay(3000);
    ESP.restart();
  } else {
    Serial.println("✅ Terhubung ke WiFi!");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    Serial.print("Hostname: ");
    Serial.println(WiFi.getHostname());
  }
}

void printSystemInfo() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("=== SYSTEM INFORMATION ===");
    Serial.print("🕒 RTC Status: ");
    Serial.println(rtcAvailable ? "✅ Connected" : "❌ Disconnected");
    if (rtcAvailable) {
      DateTime now = rtc.now();
      Serial.print("📅 RTC Time: ");
      Serial.println(getDateTimeString(now));
      Serial.print("🔁 Time Sync: ");
      Serial.println(timeSynced ? "✅ Synced" : "❌ Not Synced");
    }
    Serial.print("💾 SD Card: ");
    Serial.println(sdCardAvailable ? "✅ Connected" : "❌ Disconnected");
    Serial.println("==================================");
    Serial.println();
  }
}

void saveConfigCallback() {
  Serial.println("Konfigurasi WiFi disimpan");
  shouldSaveConfig = true;
}

void baca_registers_statistik() {
  static uint32_t loopCount = 0;
  loopCount++;

  Serial.print("Pembacaan ke-");
  Serial.println(loopCount);
  Serial.println("----------------------------------");

  for (int i = 0; i < numRegisters; i++) {
    // Dapatkan nilai sebagai float
    float floatValue = getFloatData(registersToRead[i].address, registersToRead[i].dataType);
    
    // Terapkan faktor skala
    float scaledValue = floatValue / registersToRead[i].scaleFactor;

    // Simpan ke array readingResults
    readingResults[i].name = registersToRead[i].name;
    readingResults[i].jsonKey = registersToRead[i].jsonKey;
    readingResults[i].value = scaledValue;

    // Tampilkan hasil
    Serial.print(registersToRead[i].name);
    Serial.print(" (Reg 0x");
    Serial.print(registersToRead[i].address, HEX);

    int numRegsToRead = 1;
    if (strcmp(registersToRead[i].dataType, "U32") == 0 || strcmp(registersToRead[i].dataType, "S32") == 0 || strcmp(registersToRead[i].dataType, "FLOAT") == 0 || strcmp(registersToRead[i].dataType, "U32LE") == 0 || strcmp(registersToRead[i].dataType, "S32LE") == 0 || strcmp(registersToRead[i].dataType, "FLOATLE") == 0) {
      numRegsToRead = 2;
      Serial.print("-0x");
      uint16_t endAddr = registersToRead[i].address + 1;
      Serial.print(endAddr, HEX);
    }
    Serial.print("): ");
    Serial.print(floatValue);
    Serial.print(" -> ");
    Serial.print(scaledValue, 6);
    Serial.print(" (Skala 1/");
    Serial.print(registersToRead[i].scaleFactor);
    Serial.println(")");
  }
}

void kirim_data_ke_appscript() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  if (!http.begin(client, GOOGLE_SCRIPT_URL)) {
    Serial.println("❌ Gagal memulai koneksi HTTPS");
    simpan_data_ke_sd(); // Simpan ke SD jika gagal kirim
    return;
  }

  http.addHeader("Content-Type", "application/json");

  // Buat payload JSON
  DynamicJsonDocument doc(2048);
  
  // Tambahkan timestamp dari RTC jika available
  if (rtcAvailable) {
    DateTime now = rtc.now();
    doc["timestamp"] = now.unixtime();
    doc["datetime"] = getDateTimeString(now);
  } else {
    doc["timestamp"] = millis();
  }
  
  // Tambahkan semua data
  for (int i = 0; i < numRegisters; i++) {
    doc[readingResults[i].jsonKey] = readingResults[i].value;
  }

  String payload;
  serializeJson(doc, payload);

  Serial.print("📤 Mengirim data: ");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("✅ Kode respons HTTP: %d\n", httpCode);
    String response = http.getString();
    Serial.println("📥 Respons: " + response);
    
    // Simpan data ke CSV sebagai backup meskipun berhasil dikirim
    simpan_data_ke_csv();
  } else {
    Serial.printf("❌ Error pada request: %s\n", http.errorToString(httpCode).c_str());
    simpan_data_ke_sd(); // Simpan ke SD jika gagal kirim
  }

  http.end();
}

void simpan_data_ke_sd() {
  if (!sdCardAvailable) {
    Serial.println("❌ SD Card tidak tersedia untuk penyimpanan offline");
    return;
  }

  File file = SD.open(backupFileName, FILE_APPEND);
  if (!file) {
    Serial.println("❌ Gagal membuka file backup");
    return;
  }

  // Buat string data dalam format JSON untuk penyimpanan offline
  String dataLine = "{";
  
  // Timestamp
  if (rtcAvailable) {
    DateTime now = rtc.now();
    dataLine += "\"timestamp\":" + String(now.unixtime()) + ",";
    dataLine += "\"datetime\":\"" + getDateTimeString(now) + "\",";
  } else {
    dataLine += "\"timestamp\":" + String(millis()) + ",";
  }
  
  // Data modbus
  for (int i = 0; i < numRegisters; i++) {
    dataLine += "\"" + String(readingResults[i].jsonKey) + "\":" + String(readingResults[i].value, 6);
    if (i < numRegisters - 1) dataLine += ",";
  }
  dataLine += "}\n";

  file.print(dataLine);
  file.close();
  
  Serial.println("💾 Data disimpan ke SD Card (backup)");
}

void simpan_data_ke_csv() {
  if (!sdCardAvailable) return;

  File file = SD.open(dataFileName, FILE_APPEND);
  if (!file) return;

  // Timestamp
  if (rtcAvailable) {
    DateTime now = rtc.now();
    file.print(getDateTimeString(now));
  } else {
    file.print(millis());
  }
  file.print(",");

  // Data modbus
  for (int i = 0; i < numRegisters; i++) {
    file.print(readingResults[i].value, 6);
    if (i < numRegisters - 1) file.print(",");
  }
  file.println();
  file.close();
  
  Serial.println("💾 Data disimpan ke CSV");
}

void createCSVHeader() {
  if (!sdCardAvailable) return;

  // Cek apakah file sudah ada
  if (!SD.exists(dataFileName)) {
    File file = SD.open(dataFileName, FILE_WRITE);
    if (file) {
      // Tulis header
      file.print("Timestamp,");
      for (int i = 0; i < numRegisters; i++) {
        file.print(readingResults[i].jsonKey);
        if (i < numRegisters - 1) file.print(",");
      }
      file.println();
      file.close();
      Serial.println("✅ File CSV dibuat dengan header");
    }
  }

  // Buat file backup jika belum ada
  if (!SD.exists(backupFileName)) {
    File file = SD.open(backupFileName, FILE_WRITE);
    if (file) {
      file.println("Backup Data - JSON Format");
      file.close();
    }
  }
}

void sendOfflineData() {
  if (!sdCardAvailable || !SD.exists(backupFileName)) return;

  File file = SD.open(backupFileName);
  if (!file) return;

  Serial.println("🔄 Mengirim data offline yang tersimpan...");

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String line;
  int successCount = 0;
  int failCount = 0;

  while (file.available()) {
    line = file.readStringUntil('\n');
    line.trim();
    
    if (line.length() == 0 || line.startsWith("Backup")) continue;

    if (!http.begin(client, GOOGLE_SCRIPT_URL)) {
      Serial.println("❌ Gagal memulai koneksi HTTPS untuk data offline");
      break;
    }

    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(line);

    if (httpCode > 0) {
      Serial.printf("✅ Data offline terkirim: HTTP %d\n", httpCode);
      successCount++;
    } else {
      Serial.printf("❌ Gagal kirim data offline: %s\n", http.errorToString(httpCode).c_str());
      failCount++;
      break; // Stop jika gagal, mungkin jaringan down lagi
    }

    http.end();
    delay(100); // Delay kecil antara pengiriman
  }

  file.close();

  // Hapus file backup jika semua data berhasil dikirim
  if (failCount == 0 && successCount > 0) {
    SD.remove(backupFileName);
    Serial.println("✅ Semua data offline berhasil dikirim, file backup dihapus");
  }

  Serial.printf("📊 Statistik data offline: %d berhasil, %d gagal\n", successCount, failCount);
}

String getDateTimeString(DateTime dt) {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", 
          dt.year(), dt.month(), dt.day(), 
          dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}

// Fungsi untuk reset WiFi configuration (bisa dipanggil via serial)
void resetWiFiConfig() {
  Serial.println("🗑️ Menghapus konfigurasi WiFi...");
  wm.resetSettings();
  delay(1000);
  Serial.println("🔄 Restarting ESP32...");
  ESP.restart();
}

// Fungsi untuk membuka configuration portal manual
void startConfigPortal() {
  Serial.println("🚀 Membuka Configuration Portal...");
  wm.setConfigPortalTimeout(180);
  
  if (!wm.startConfigPortal("ESP32-DataLogger", "password123")) {
    Serial.println("❌ Gagal connect dan timeout");
  } else {
    Serial.println("✅ Terhubung ke WiFi baru!");
  }
}

// Fungsi-fungsi modbus yang sudah ada (getFloatData, getRawData, printModbusError, convertToFloat)
float getFloatData(uint16_t address, const char* dataType) {
  int numRegsToRead = 1;
  if (strcmp(dataType, "U32") == 0 || strcmp(dataType, "S32") == 0 || strcmp(dataType, "FLOAT") == 0 || strcmp(dataType, "U32LE") == 0 || strcmp(dataType, "S32LE") == 0 || strcmp(dataType, "FLOATLE") == 0) {
    numRegsToRead = 2;
  }

  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(address, numRegsToRead);

  if (result != node.ku8MBSuccess) {
    Serial.print("Error membaca register 0x");
    Serial.print(address, HEX);
    Serial.print(": ");
    printModbusError(result);
    return NAN;
  }

  if (strcmp(dataType, "U16") == 0) {
    return (float)node.getResponseBuffer(0);
  } else if (strcmp(dataType, "S16") == 0) {
    return (float)((int16_t)node.getResponseBuffer(0));
  } else if (strcmp(dataType, "U32") == 0) {
    uint32_t value = ((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);
    return (float)value;
  } else if (strcmp(dataType, "S32") == 0) {
    int32_t value = ((int32_t)(int16_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);
    return (float)value;
  } else if (strcmp(dataType, "FLOAT") == 0) {
    return convertToFloat(node.getResponseBuffer(0), node.getResponseBuffer(1));
  } else if (strcmp(dataType, "U32LE") == 0) {
    uint32_t value = ((uint32_t)node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return (float)value;
  } else if (strcmp(dataType, "S32LE") == 0) {
    int32_t value = ((int32_t)(int16_t)node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return (float)value;
  } else if (strcmp(dataType, "FLOATLE") == 0) {
    return convertToFloat(node.getResponseBuffer(1), node.getResponseBuffer(0));
  }

  Serial.print("Tipe data tidak dikenal: ");
  Serial.println(dataType);
  return NAN;
}

bool getRawData(uint16_t address, int numRegisters, uint16_t* buffer) {
  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(address, numRegisters);

  if (result != node.ku8MBSuccess) {
    return false;
  }

  for (int i = 0; i < numRegisters; i++) {
    buffer[i] = node.getResponseBuffer(i);
  }

  return true;
}

void printModbusError(uint8_t result) {
  switch (result) {
    case node.ku8MBIllegalFunction:
      Serial.println("Illegal Function");
      break;
    case node.ku8MBIllegalDataAddress:
      Serial.println("Illegal Data Address");
      break;
    case node.ku8MBIllegalDataValue:
      Serial.println("Illegal Data Value");
      break;
    case node.ku8MBSlaveDeviceFailure:
      Serial.println("Slave Device Failure");
      break;
    case node.ku8MBInvalidSlaveID:
      Serial.println("Invalid Slave ID");
      break;
    case node.ku8MBInvalidFunction:
      Serial.println("Invalid Function");
      break;
    case node.ku8MBResponseTimedOut:
      Serial.println("Response Timeout");
      break;
    case node.ku8MBInvalidCRC:
      Serial.println("Invalid CRC");
      break;
    default:
      Serial.print("Unknown Error: ");
      Serial.println(result);
      break;
  }
}

float convertToFloat(uint16_t highWord, uint16_t lowWord) {
  union {
    uint32_t i;
    float f;
  } converter;

  converter.i = ((uint32_t)highWord << 16) | lowWord;
  return converter.f;
}