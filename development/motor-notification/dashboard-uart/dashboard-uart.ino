#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

// ========================================================
// =============== KONFIGURASI POSISI LAYAR ===============
// ========================================================

// === HEADER (Jam & Tanggal) ===
#define HEADER_HEIGHT 55      
#define JAM_POS_Y 8           
#define TANGGAL_POS_Y 58      
#define GARIS_AKSEN_POS_Y 75  

// === CARD NAVIGASI ===
#define NAV_CARD_Y 82       
#define NAV_CARD_HEIGHT 75  
#define NAV_TITLE_Y 8       
#define NAV_INSTR_Y 28      
#define NAV_DIST_ETA_Y 52   

// === CARD NOTIFIKASI ===
#define NOTIF_CARD_Y 82        
#define NOTIF_CARD_HEIGHT 95   
#define NOTIF_SRC_Y 10         
#define NOTIF_TITLE_Y 25       
#define NOTIF_BODY_LINE1_Y 45  
#define NOTIF_BODY_LINE2_Y 60  
#define NOTIF_BODY_LINE3_Y 75  

// === CARD MUSIK ===
#define MUSIC_CARD_HEIGHT 85         
#define MUSIC_CARD_Y_DEFAULT 110     
#define MUSIC_CARD_Y_WITH_NOTIF 182  
#define MUSIC_CARD_Y_WITH_NAV 162    
#define MUSIC_TITLE_Y 10             
#define MUSIC_ARTIST_Y 30            
#define MUSIC_PROGRESS_Y 42          
#define MUSIC_TIME_Y 52              
#define MUSIC_STATUS_Y 65            

// === CARD PANGGILAN ===
#define CALL_CARD_HEIGHT 70         
#define CALL_CARD_Y_DEFAULT 110     
#define CALL_CARD_Y_WITH_NOTIF 182  
#define CALL_CARD_Y_WITH_NAV 162    
#define CALL_TITLE_Y 8              
#define CALL_NAME_Y 32              
#define CALL_INSTRUCTION_Y 55       

// === VOLUME BAR ===
#define VOLUME_Y_FROM_BOTTOM 45  
#define VOLUME_BAR_Y 10          
#define VOLUME_PERCENT_Y 18      

// === FOOTER ===
#define FOOTER_Y_FROM_BOTTOM 10  
#define FOOTER_LINE_OFFSET 3     

// === STANDBY MODE ===
#define STANDBY_JAM_Y 70       
#define STANDBY_TANGGAL_Y 130  
#define STANDBY_TEXT_Y 200     

// ========================================================
// =============== KONFIGURASI LDR ===============
// ========================================================
#define LDR_PIN 34

// ========================================================
// =============== KONFIGURASI TRIP DATA ===============
// ========================================================
// Protokol komunikasi dengan Arduino
#define CMD_PREFIX "CMD:"      // Prefix untuk perintah ke Arduino
#define DATA_PREFIX "DATA:"    // Prefix untuk data dari Arduino
#define CFG_PREFIX "CFG:"      // Prefix untuk konfigurasi

// Struktur data trip
struct TripData {
  unsigned long startTimestamp;
  unsigned long lastUpdateTimestamp;
  bool engineStatus;
  unsigned long tripDuration; // dalam detik
  float threshold;
  int detectionWindow;
  int minVibration;
};

// ========================================================
// Warna Tema
// ========================================================
uint16_t COLOR_CARD_BG = 0x3186;
uint16_t COLOR_BG_BOTTOM = 0x0000;
uint16_t COLOR_ACCENT = 0x05FF;
uint16_t COLOR_SUCCESS = 0x07E0;
uint16_t COLOR_WARNING = 0xFD20;
uint16_t COLOR_DANGER = 0xF800;
uint16_t COLOR_PRIMARY = 0x05FF;
uint16_t COLOR_GLASS = 0xC618;
uint16_t COLOR_TEXT_PRIMARY = TFT_WHITE;
uint16_t COLOR_TEXT_SECONDARY = 0x7BEF;

const uint16_t COLOR_CARD_BG_DARK = 0x3186;
const uint16_t COLOR_BG_BOTTOM_DARK = 0x0000;
const uint16_t COLOR_ACCENT_DARK = 0x05FF;
const uint16_t COLOR_PRIMARY_DARK = 0x05FF;
const uint16_t COLOR_GLASS_DARK = 0xC618;
const uint16_t COLOR_TEXT_PRIMARY_DARK = TFT_WHITE;
const uint16_t COLOR_TEXT_SECONDARY_DARK = 0x7BEF;

const uint16_t COLOR_CARD_BG_LIGHT = 0xE73C;
const uint16_t COLOR_BG_BOTTOM_LIGHT = 0xEF5D;
const uint16_t COLOR_ACCENT_LIGHT = 0x001F;
const uint16_t COLOR_PRIMARY_LIGHT = 0x001F;
const uint16_t COLOR_GLASS_LIGHT = 0xCE79;
const uint16_t COLOR_TEXT_PRIMARY_LIGHT = TFT_BLACK;
const uint16_t COLOR_TEXT_SECONDARY_LIGHT = 0x528A;
const uint16_t COLOR_SUCCESS_LIGHT = 0x04A0;

bool isDarkMode = true;

// ========================================================
// BLE & Data Variables
// ========================================================
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define SD_CS 5

BLECharacteristic *pCharacteristicTX;
bool deviceConnected = false;
bool lastConnectionState = false;
String artist = "-", title = "-", currentTime = "--:--";
String navInstr = "", navDist = "", navEta = "";
String musicState = "pause";
int volumeLevel = 0;
long positionSec = 0, durationSec = 0;
time_t lastUnixTime = 0;
String inputBuffer = "";
bool refreshDisplay = true;
bool isNavigating = false;

// Variabel tracking
String lastArtist = "", lastTitle = "", lastMusicState = "";
String lastNavInstr = "", lastNavDist = "", lastNavEta = "";
int lastVolumeLevel = -1;
long lastPositionSec = -1;
bool lastIsNavigating = false;
bool lastIsNotify = false;
bool lastIsIncomingCall = false;
String lastCurrentTime = "";
String lastNotifySrc = "", lastNotifyTitle = "", lastNotifyBody = "";
String lastCallName = "";

// Variabel UI
bool isNotify = false;
String notifySrc = "", notifyTitle = "", notifyBody = "";
bool isIncomingCall = false;
String callName = "";
unsigned long lastNotifyTime = 0;

// Timer
unsigned long lastClockCheck = 0;
unsigned long lastSDWrite = 0;

// Variabel untuk data trip
bool currentEngineStatus = false;
unsigned long currentTripDuration = 0;
float currentThreshold = 12.0;
int currentDetectionWindow = 2000;
int currentMinVibration = 15;
String tripDataBuffer = "";
bool hasCreatedTripLog = false;
unsigned long tripStartTime = 0;
unsigned long lastTripUpdate = 0;

const char *hariIndo[] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
const char *bulanIndo[] = { "Januari", "Februari", "Maret", "April", "Mei", "Juni",
                            "Juli", "Agustus", "September", "Oktober", "November", "Desember" };

// ========================================================
// FUNGSI BANTU POSISI
// ========================================================
int getMusicCardY() {
  if (isNavigating) return MUSIC_CARD_Y_WITH_NAV;
  if (isNotify) return MUSIC_CARD_Y_WITH_NOTIF;
  return MUSIC_CARD_Y_DEFAULT;
}

int getCallCardY() {
  if (isNavigating) return CALL_CARD_Y_WITH_NAV;
  if (isNotify) return CALL_CARD_Y_WITH_NOTIF;
  return CALL_CARD_Y_DEFAULT;
}

int getVolumeY() {
  return tft.height() - VOLUME_Y_FROM_BOTTOM;
}

int getFooterY() {
  return tft.height() - FOOTER_Y_FROM_BOTTOM;
}

// ========================================================
// FUNGSI UTILITY
// ========================================================
String formatTime(long seconds) {
  if (seconds < 0) seconds = 0;
  int m = seconds / 60;
  int s = seconds % 60;
  char buf[10];
  sprintf(buf, "%02d:%02d", m, s);
  return String(buf);
}

String formatDuration(unsigned long seconds) {
  unsigned long h = seconds / 3600;
  unsigned long m = (seconds % 3600) / 60;
  unsigned long s = seconds % 60;
  char buf[20];
  sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ========================================================
// FUNGSI KOMUNIKASI DENGAN ARDUINO VIA SERIAL
// ========================================================

// Kirim perintah ke Arduino dengan prefix
void sendToArduino(String command) {
  Serial.print(CMD_PREFIX);
  Serial.println(command);
  Serial.flush();
  Serial.println("[CYD->Arduino] " + command);
}

// Kirim konfigurasi ke Arduino
void sendConfigToArduino() {
  StaticJsonDocument<256> doc;
  JsonArray setArray = doc.createNestedArray("set");
  setArray.add(currentThreshold);
  setArray.add(currentDetectionWindow);
  setArray.add(currentMinVibration);
  
  String output;
  serializeJson(doc, output);
  
  sendToArduino(output);
  Serial.print("[CFG] Konfigurasi dikirim ke Arduino: ");
  Serial.println(output);
}

// Kirim ping ke Arduino untuk cek koneksi
void sendPingToArduino() {
  sendToArduino("ping");
}

// Proses data yang diterima dari Arduino (sudah tanpa prefix)
void processArduinoData(String data) {
  Serial.print("[Arduino->CYD] ");
  Serial.println(data);
  
  // Parse JSON dari Arduino
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);
  
  if (error) {
    Serial.print("[ERROR] JSON parse: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Ekstrak data engine status dan trip duration
  int engStatus = doc["eng"] | -1;
  unsigned long tripDuration = doc["trip"] | 0;
  
  if (engStatus != -1) {
    bool newEngineStatus = (engStatus == 1);
    
    // Cek jika ada perubahan status engine
    if (newEngineStatus != currentEngineStatus) {
      currentEngineStatus = newEngineStatus;
      
      if (currentEngineStatus) {
        tripStartTime = millis();
        Serial.println("[TRIP] Engine STARTED");
        
        // Tampilkan notifikasi
        notifySrc = "Engine System";
        notifyTitle = "Engine STARTED";
        notifyBody = "Recording trip data...";
        isNotify = true;
        lastNotifyTime = millis();
        refreshDisplay = true;
      } else {
        Serial.println("[TRIP] Engine STOPPED");
        
        // Tampilkan notifikasi dengan total durasi
        notifySrc = "Engine System";
        notifyTitle = "Engine STOPPED";
        notifyBody = "Total trip: " + formatDuration(tripDuration);
        isNotify = true;
        lastNotifyTime = millis();
        refreshDisplay = true;
      }
    }
    
    currentTripDuration = tripDuration;
    
    // Update file CSV setiap ada perubahan data
    updateLastTripLogEntry();
    
    Serial.print("[TRIP] Engine: ");
    Serial.print(currentEngineStatus ? "ON" : "OFF");
    Serial.print(" | Trip: ");
    Serial.println(formatDuration(currentTripDuration));
  }
}

// ========================================================
// FUNGSI LDR & MANAJEMEN TEMA
// ========================================================
void updateThemeColors() {
  if (isDarkMode) {
    COLOR_CARD_BG = COLOR_CARD_BG_DARK;
    COLOR_BG_BOTTOM = COLOR_BG_BOTTOM_DARK;
    COLOR_ACCENT = COLOR_ACCENT_DARK;
    COLOR_PRIMARY = COLOR_PRIMARY_DARK;
    COLOR_GLASS = COLOR_GLASS_DARK;
    COLOR_TEXT_PRIMARY = COLOR_TEXT_PRIMARY_DARK;
    COLOR_TEXT_SECONDARY = COLOR_TEXT_SECONDARY_DARK;
    COLOR_SUCCESS = 0x07E0;
  } else {
    COLOR_CARD_BG = COLOR_CARD_BG_LIGHT;
    COLOR_BG_BOTTOM = COLOR_BG_BOTTOM_LIGHT;
    COLOR_ACCENT = COLOR_ACCENT_LIGHT;
    COLOR_PRIMARY = COLOR_PRIMARY_LIGHT;
    COLOR_GLASS = COLOR_GLASS_LIGHT;
    COLOR_TEXT_PRIMARY = COLOR_TEXT_PRIMARY_LIGHT;
    COLOR_TEXT_SECONDARY = COLOR_TEXT_SECONDARY_LIGHT;
    COLOR_SUCCESS = COLOR_SUCCESS_LIGHT;
  }
}

void checkLDRAndUpdateTheme() {
  int ldrValue = analogRead(LDR_PIN);
  bool newDarkMode;
  
  if (ldrValue == 0) {
    newDarkMode = false;
    Serial.println("[LDR] Mode TERANG (Siang)");
  } else {
    newDarkMode = true;
    Serial.print("[LDR] Mode GELAP (Malam) - Nilai: ");
    Serial.println(ldrValue);
  }
  
  if (newDarkMode != isDarkMode) {
    isDarkMode = newDarkMode;
    updateThemeColors();
    Serial.println("[LDR] Tema berubah! ✅");
    refreshDisplay = true;
  }
}

uint16_t getBgColor(int i, int height) {
  if (isDarkMode) {
    uint8_t factor = (i * 255) / height;
    uint8_t r = 5 + (factor * 15 / 255);
    uint8_t g = 5 + (factor * 10 / 255);
    uint8_t b = 20 + (factor * 30 / 255);
    return color565(r, g, b);
  } else {
    uint8_t factor = (i * 255) / height;
    uint8_t r = 245 - (factor * 20 / 255);
    uint8_t g = 245 - (factor * 20 / 255);
    uint8_t b = 245 - (factor * 20 / 255);
    return color565(r, g, b);
  }
}

// ========================================================
// FUNGSI MANAJEMEN SD CARD UNTUK TRIP DATA
// ========================================================

// Mendapatkan jumlah baris dalam file CSV
int getCSVRowCount() {
  if (!SD.begin(SD_CS)) return 0;
  
  File file = SD.open("/trip_log.csv", FILE_READ);
  if (!file) return 0;
  
  int rowCount = 0;
  while (file.available()) {
    if (file.read() == '\n') rowCount++;
  }
  file.close();
  
  return rowCount;
}

// Membuat baris baru di awal session (hanya sekali saat boot)
void createNewTripLogEntry() {
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] SD Card tidak tersedia");
    return;
  }
  
  // Cek apakah file sudah ada
  bool fileExists = SD.exists("/trip_log.csv");
  
  // Buka file untuk append atau create
  File file = SD.open("/trip_log.csv", FILE_APPEND);
  if (!file) {
    Serial.println("[SD] Gagal membuka trip_log.csv");
    return;
  }
  
  // Jika file baru, tulis header
  if (!fileExists) {
    file.println("Session_Start,Last_Update,Engine_Status,Trip_Duration_Sec,Trip_Duration_Formatted,Threshold,Detection_Window,Min_Vibration");
    Serial.println("[SD] Header CSV dibuat");
  }
  
  // Dapatkan timestamp saat boot
  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);
  
  char sessionStart[64];
  sprintf(sessionStart, "%04d-%02d-%02d %02d:%02d:%02d", 
          timeinfo->tm_year + 1900, 
          timeinfo->tm_mon + 1, 
          timeinfo->tm_mday,
          timeinfo->tm_hour,
          timeinfo->tm_min,
          timeinfo->tm_sec);
  
  // Tulis baris baru dengan data awal
  file.print(sessionStart);
  file.print(",");
  file.print(sessionStart);  // Last update sama dengan start
  file.print(",");
  file.print("WAITING");
  file.print(",");
  file.print("0");
  file.print(",");
  file.print("00:00:00");
  file.print(",");
  file.print(currentThreshold);
  file.print(",");
  file.print(currentDetectionWindow);
  file.print(",");
  file.println(currentMinVibration);
  
  file.close();
  hasCreatedTripLog = true;
  
  Serial.print("[SD] Baris baru dibuat: ");
  Serial.println(sessionStart);
}

// Memperbarui baris terakhir di file CSV
void updateLastTripLogEntry() {
  if (!hasCreatedTripLog) {
    // Jika belum ada baris, buat dulu
    createNewTripLogEntry();
    return;
  }
  
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] SD Card tidak tersedia untuk update");
    return;
  }
  
  File file = SD.open("/trip_log.csv", FILE_READ);
  if (!file) {
    Serial.println("[SD] Gagal membaca trip_log.csv");
    return;
  }
  
  // Baca semua baris ke dalam array
  String lines[100]; // Maksimal 100 baris
  int lineCount = 0;
  
  while (file.available() && lineCount < 100) {
    lines[lineCount] = file.readStringUntil('\n');
    lineCount++;
  }
  file.close();
  
  if (lineCount == 0) {
    Serial.println("[SD] File kosong");
    return;
  }
  
  // Dapatkan timestamp saat ini
  time_t now;
  time(&now);
  struct tm *timeinfo = localtime(&now);
  
  char lastUpdate[64];
  sprintf(lastUpdate, "%04d-%02d-%02d %02d:%02d:%02d", 
          timeinfo->tm_year + 1900, 
          timeinfo->tm_mon + 1, 
          timeinfo->tm_mday,
          timeinfo->tm_hour,
          timeinfo->tm_min,
          timeinfo->tm_sec);
  
  // Ambil session_start dari baris terakhir (kolom pertama)
  int lastIndex = lineCount - 1;
  String lastLine = lines[lastIndex];
  int firstComma = lastLine.indexOf(',');
  String sessionStart = (firstComma > 0) ? lastLine.substring(0, firstComma) : lastUpdate;
  
  // Buat baris baru yang sudah diupdate
  String newLine = sessionStart;
  newLine += ",";
  newLine += lastUpdate;
  newLine += ",";
  newLine += currentEngineStatus ? "ON" : "OFF";
  newLine += ",";
  newLine += String(currentTripDuration);
  newLine += ",";
  newLine += formatDuration(currentTripDuration);
  newLine += ",";
  newLine += String(currentThreshold);
  newLine += ",";
  newLine += String(currentDetectionWindow);
  newLine += ",";
  newLine += String(currentMinVibration);
  
  // Tulis ulang file dengan baris terakhir yang diupdate
  file = SD.open("/trip_log.csv", FILE_WRITE);
  if (!file) {
    Serial.println("[SD] Gagal membuka file untuk write");
    return;
  }
  
  // Tulis semua baris kecuali baris terakhir
  for (int i = 0; i < lastIndex; i++) {
    file.println(lines[i]);
  }
  // Tulis baris terakhir yang sudah diupdate
  file.println(newLine);
  
  file.close();
  
  Serial.print("[SD] Baris terakhir diupdate: ");
  Serial.print(currentEngineStatus ? "ON" : "OFF");
  Serial.print(" | Durasi: ");
  Serial.println(formatDuration(currentTripDuration));
}

// ========================================================
// CEK PERUBAHAN DATA
// ========================================================
bool hasDataChanged() {
  bool changed = false;
  if (lastCurrentTime != currentTime) {
    lastCurrentTime = currentTime;
    changed = true;
  }
  if (lastTitle != title || lastArtist != artist || lastMusicState != musicState) {
    lastTitle = title;
    lastArtist = artist;
    lastMusicState = musicState;
    changed = true;
  }
  if (musicState == "pause" && lastPositionSec != positionSec) {
    lastPositionSec = positionSec;
    changed = true;
  }
  if (lastNavInstr != navInstr || lastNavDist != navDist || lastNavEta != navEta || lastIsNavigating != isNavigating) {
    lastNavInstr = navInstr;
    lastNavDist = navDist;
    lastNavEta = navEta;
    lastIsNavigating = isNavigating;
    changed = true;
  }
  if (lastVolumeLevel != volumeLevel) {
    lastVolumeLevel = volumeLevel;
    changed = true;
  }
  if (lastIsNotify != isNotify) {
    lastIsNotify = isNotify;
    changed = true;
  }
  if (isNotify && (lastNotifySrc != notifySrc || lastNotifyTitle != notifyTitle || lastNotifyBody != notifyBody)) {
    lastNotifySrc = notifySrc;
    lastNotifyTitle = notifyTitle;
    lastNotifyBody = notifyBody;
    changed = true;
  }
  if (lastIsIncomingCall != isIncomingCall) {
    lastIsIncomingCall = isIncomingCall;
    changed = true;
  }
  if (isIncomingCall && lastCallName != callName) {
    lastCallName = callName;
    changed = true;
  }
  return changed;
}

// ========================================================
// MANAJEMEN SD CARD
// ========================================================
void loadTimeFromSD() {
  File file = SD.open("/config.txt", FILE_READ);
  if (!file) {
    Serial.println("[SD] config.txt tidak ditemukan");
    return;
  }
  String savedTimeStr = file.readStringUntil('\n');
  file.close();
  time_t savedTimestamp = (time_t)savedTimeStr.toInt();
  if (savedTimestamp > 1000000000) {
    lastUnixTime = savedTimestamp;
    struct timeval tv;
    tv.tv_sec = lastUnixTime;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    struct tm *tmp = localtime(&lastUnixTime);
    char buf[10];
    sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
    currentTime = String(buf);
    lastCurrentTime = currentTime;
    Serial.println("[SD] Waktu dimuat dari SD Card");
  }
}

void saveTimeToSD() {
  time_t now;
  time(&now);
  if (now > 1600000000) {
    File file = SD.open("/config.txt", FILE_WRITE);
    if (file) {
      file.println(now);
      file.close();
    }
  }
}

// ========================================================
// EFEK VISUAL
// ========================================================
void drawGradientBackground() {
  for (int i = 0; i < tft.height(); i++) {
    tft.drawFastHLine(0, i, tft.width(), getBgColor(i, tft.height()));
  }
}

void drawGlassEffect(int x, int y, int w, int h, int radius = 12) {
  tft.fillRoundRect(x, y, w, h, radius, COLOR_CARD_BG);
  if (!isDarkMode) {
    tft.drawRoundRect(x, y, w, h, radius, COLOR_GLASS);
  }
}

void drawBorderWithColor(int x, int y, int w, int h, int radius, uint16_t color) {
  tft.drawRoundRect(x, y, w, h, radius, color);
  tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, radius - 1, color);
}

void drawNeonProgressBar(int x, int y, int width, int height, float progress, uint16_t color) {
  tft.fillRoundRect(x, y, width, height, height / 2, 0x2104);
  tft.drawRoundRect(x, y, width, height, height / 2, color);
  int fillWidth = width * progress;
  if (fillWidth > 0) { tft.fillRoundRect(x, y, fillWidth, height, height / 2, color); }
}

// ========================================================
// KOMPONEN UI
// ========================================================
void drawModernHeader() {
  for (int i = 0; i < HEADER_HEIGHT; i++) {
    if (isDarkMode) {
      tft.drawFastHLine(0, i, tft.width(), color565(10, 10, 30));
    } else {
      tft.drawFastHLine(0, i, tft.width(), color565(240, 240, 245));
    }
  }
  
  uint16_t bgColor = isDarkMode ? color565(10, 10, 30) : color565(240, 240, 245);
  
  tft.setTextColor(COLOR_TEXT_PRIMARY, bgColor);
  tft.drawCentreString(currentTime, tft.width() / 2, JAM_POS_Y, 6);

  if (lastUnixTime > 0) {
    struct tm *tmp = localtime(&lastUnixTime);
    char dateBuf[50];
    sprintf(dateBuf, "%s, %02d %s %d", hariIndo[tmp->tm_wday], tmp->tm_mday,
            bulanIndo[tmp->tm_mon], tmp->tm_year + 1900);
    tft.setTextColor(COLOR_TEXT_SECONDARY, bgColor);
    tft.drawCentreString(dateBuf, tft.width() / 2, TANGGAL_POS_Y, 2);
  }
  tft.drawRect(20, GARIS_AKSEN_POS_Y, tft.width() - 40, 2, COLOR_ACCENT);
}

void drawNavigationCard() {
  drawGlassEffect(10, NAV_CARD_Y, tft.width() - 20, NAV_CARD_HEIGHT, 10);
  drawBorderWithColor(10, NAV_CARD_Y, tft.width() - 20, NAV_CARD_HEIGHT, 10, COLOR_PRIMARY);

  tft.setTextColor(COLOR_PRIMARY, COLOR_CARD_BG);
  tft.drawCentreString("NAVIGASI", tft.width() / 2, NAV_CARD_Y + NAV_TITLE_Y, 1);

  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  String displayInstr = navInstr;
  if (displayInstr.length() > 28) displayInstr = displayInstr.substring(0, 25) + "...";
  tft.drawCentreString(displayInstr, tft.width() / 2, NAV_CARD_Y + NAV_INSTR_Y, 2);

  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawString(" " + navDist, 20, NAV_CARD_Y + NAV_DIST_ETA_Y, 2);
  tft.drawRightString(navEta + " ", tft.width() - 20, NAV_CARD_Y + NAV_DIST_ETA_Y, 2);
}

void drawNotificationPopup() {
  if (!isNotify) return;

  drawGlassEffect(10, NOTIF_CARD_Y, tft.width() - 20, NOTIF_CARD_HEIGHT, 10);
  drawBorderWithColor(10, NOTIF_CARD_Y, tft.width() - 20, NOTIF_CARD_HEIGHT, 10, COLOR_DANGER);

  tft.setTextColor(COLOR_DANGER, COLOR_CARD_BG);
  tft.drawString(notifySrc, 20, NOTIF_CARD_Y + NOTIF_SRC_Y, 1);

  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  String shortTitle = notifyTitle.length() > 30 ? notifyTitle.substring(0, 27) + "..." : notifyTitle;
  tft.drawString(shortTitle, 20, NOTIF_CARD_Y + NOTIF_TITLE_Y, 2);

  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  String bodyText = notifyBody;
  String line1 = "", line2 = "", line3 = "";

  if (bodyText.length() > 0) {
    int maxChars = 34;

    if (bodyText.length() > maxChars) {
      line1 = bodyText.substring(0, maxChars);
      String remaining = bodyText.substring(maxChars);

      if (remaining.length() > maxChars) {
        line2 = remaining.substring(0, maxChars);
        String remaining2 = remaining.substring(maxChars);

        if (remaining2.length() > 0) {
          if (remaining2.length() > maxChars - 3) {
            line3 = remaining2.substring(0, maxChars - 3) + "...";
          } else {
            line3 = remaining2;
          }
        }
      } else {
        line2 = remaining;
      }
    } else {
      line1 = bodyText;
    }

    if (line1.length() > 0) tft.drawString(line1, 20, NOTIF_CARD_Y + NOTIF_BODY_LINE1_Y, 1);
    if (line2.length() > 0) tft.drawString(line2, 20, NOTIF_CARD_Y + NOTIF_BODY_LINE2_Y, 1);
    if (line3.length() > 0) tft.drawString(line3, 20, NOTIF_CARD_Y + NOTIF_BODY_LINE3_Y, 1);
  }
}

void drawMusicPlayer() {
  int musicY = getMusicCardY();
  uint16_t borderColor = (musicState == "play") ? COLOR_SUCCESS : COLOR_WARNING;

  drawGlassEffect(10, musicY, tft.width() - 20, MUSIC_CARD_HEIGHT, 10);
  drawBorderWithColor(10, musicY, tft.width() - 20, MUSIC_CARD_HEIGHT, 10, borderColor);

  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  String displayTitle = title;
  if (displayTitle.length() > 38) displayTitle = displayTitle.substring(0, 35) + "...";
  tft.drawString(displayTitle, 20, musicY + MUSIC_TITLE_Y, 2);

  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  String displayArtist = artist;
  if (displayArtist.length() > 38) displayArtist = displayArtist.substring(0, 35) + "...";
  tft.drawString(displayArtist, 20, musicY + MUSIC_ARTIST_Y, 1);

  if (musicState == "pause") {
    float progress = (durationSec > 0) ? (float)positionSec / durationSec : 0;
    drawNeonProgressBar(20, musicY + MUSIC_PROGRESS_Y, tft.width() - 40, 5, progress, COLOR_WARNING);
    tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
    tft.drawString(formatTime(positionSec) + " / " + formatTime(durationSec), 20, musicY + MUSIC_TIME_Y, 1);
    tft.setTextColor(COLOR_WARNING, COLOR_CARD_BG);
    tft.drawString("PAUSED", 20, musicY + MUSIC_STATUS_Y, 1);
  } else {
    tft.setTextColor(COLOR_SUCCESS, COLOR_CARD_BG);
    tft.drawString("NOW PLAYING", 20, musicY + MUSIC_STATUS_Y, 2);
  }
}

void drawCallCard() {
  int callY = getCallCardY();

  drawGlassEffect(10, callY, tft.width() - 20, CALL_CARD_HEIGHT, 10);
  drawBorderWithColor(10, callY, tft.width() - 20, CALL_CARD_HEIGHT, 10, COLOR_DANGER);

  tft.setTextColor(COLOR_DANGER, COLOR_CARD_BG);
  tft.drawCentreString("PANGGILAN MASUK", tft.width() / 2, callY + CALL_TITLE_Y, 2);

  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  tft.drawCentreString(callName, tft.width() / 2, callY + CALL_NAME_Y, 2);

  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawCentreString("Angkat di HP Anda", tft.width() / 2, callY + CALL_INSTRUCTION_Y, 1);
}

void drawVolumeBar() {
  int volY = getVolumeY();
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG_BOTTOM);
  tft.drawCentreString("VOLUME", tft.width() / 2, volY, 1);

  float volumeProgress = volumeLevel / 100.0;
  drawNeonProgressBar((tft.width() - 180) / 2, volY + VOLUME_BAR_Y, 180, 6, volumeProgress, COLOR_PRIMARY);

  tft.setTextColor(COLOR_PRIMARY, COLOR_BG_BOTTOM);
  tft.drawCentreString(String(volumeLevel) + "%", tft.width() / 2, volY + VOLUME_PERCENT_Y, 1);
}

void drawFooter() {
  int footerY = getFooterY();
  tft.drawFastHLine(20, footerY - FOOTER_LINE_OFFSET, tft.width() - 40, COLOR_TEXT_SECONDARY);

  // if (deviceConnected) {
  //   tft.setTextColor(COLOR_SUCCESS, COLOR_BG_BOTTOM);
  //   tft.drawString("Connected", 20, footerY, 1);
  // } else {
  //   tft.setTextColor(COLOR_DANGER, COLOR_BG_BOTTOM);
  //   tft.drawString("Disconnected", 20, footerY, 1);
  // }
  
  // Informasi engine status di footer
  if (currentEngineStatus) {
    tft.setTextColor(COLOR_SUCCESS, COLOR_BG_BOTTOM);
    tft.drawString("Engine: ON", 20, footerY, 1);
  } else {
    tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG_BOTTOM);
    tft.drawString("Engine: OFF", 20, footerY, 1);
  }
  
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG_BOTTOM);
  tft.drawRightString("Dashboard v2.0", tft.width() - 20, footerY, 1);
}

void drawStandbyMode() {
  drawGradientBackground();
  
  uint16_t bgColor = getBgColor(STANDBY_JAM_Y, tft.height());
  
  tft.setTextColor(COLOR_TEXT_PRIMARY, bgColor);
  tft.drawCentreString(currentTime, tft.width() / 2, STANDBY_JAM_Y, 6);

  if (lastUnixTime > 0) {
    struct tm *tmp = localtime(&lastUnixTime);
    char dateBuf[50];
    sprintf(dateBuf, "%s, %02d %s %d", hariIndo[tmp->tm_wday], tmp->tm_mday,
            bulanIndo[tmp->tm_mon], tmp->tm_year + 1900);
    tft.setTextColor(COLOR_TEXT_SECONDARY, bgColor);
    tft.drawCentreString(dateBuf, tft.width() / 2, STANDBY_TANGGAL_Y, 2);
  }
  tft.setTextColor(COLOR_WARNING, COLOR_BG_BOTTOM);
  tft.drawCentreString("MENUNGGU KONEKSI", tft.width() / 2, STANDBY_TEXT_Y, 2);
  drawFooter();
}

// ========================================================
// UPDATE DISPLAY
// ========================================================
void updateDisplay() {
  checkLDRAndUpdateTheme();
  
  if (!deviceConnected) {
    drawStandbyMode();
  } else {
    drawGradientBackground();
    drawModernHeader();
    if (isNavigating) drawNavigationCard();
    else if (isNotify) drawNotificationPopup();
    if (isIncomingCall) drawCallCard();
    else drawMusicPlayer();
    drawVolumeBar();
    drawFooter();
  }
  refreshDisplay = false;
}

// ========================================================
// UPDATE WAKTU
// ========================================================
void updateTimeIfNeeded() {
  time_t now;
  time(&now);
  if (now > lastUnixTime) {
    lastUnixTime = now;
    struct tm *tmp = localtime(&lastUnixTime);
    char buf[10];
    sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
    String newTime = String(buf);
    if (newTime != currentTime) {
      currentTime = newTime;
      refreshDisplay = true;
    }
  }
}

// ========================================================
// BLE & DATA PROCESSING
// ========================================================
void processBuffer(String data) {
  Serial.println("\n[PARSING] " + data);
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    lastUnixTime = data.substring(startIdx, endIdx).toInt();
    if (lastUnixTime > 0) {
      lastUnixTime += 28800;
      struct timeval tv;
      tv.tv_sec = lastUnixTime;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);
      struct tm *tmp = localtime(&lastUnixTime);
      char buf[10];
      sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
      currentTime = String(buf);
      lastCurrentTime = currentTime;
      saveTimeToSD();
      refreshDisplay = true;
    }
    return;
  }

  int start = data.indexOf("GB({");
  int end = data.lastIndexOf("})");
  if (start != -1 && end != -1) {
    String jsonStr = data.substring(start + 3, end + 2);
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, jsonStr) == DeserializationError::Ok) {
      String type = doc["t"] | "";
      if (type == "musicinfo") {
        title = doc["track"] | "No Title";
        artist = doc["artist"] | "No Artist";
        durationSec = doc["dur"] | 0;
        refreshDisplay = true;
      } else if (type == "musicstate") {
        String newState = doc["state"].as<String>();
        long newPosition = doc["position"] | 0;
        if (newState == "pause" || musicState != newState) {
          positionSec = newPosition;
          lastPositionSec = positionSec;
        }
        musicState = newState;
        refreshDisplay = true;
      } else if (type == "nav") {
        navInstr = doc["instr"] | "";
        String rawDist = doc["distance"] | "";
        if (rawDist.length() >= 2) rawDist.setCharAt(rawDist.length() - 2, ' ');
        navDist = rawDist;
        navEta = doc["eta"] | "";
        bool newNavigating = (navInstr != "" && navInstr != " " && navInstr != "null");
        if (newNavigating && !isNavigating) isNotify = false;
        isNavigating = newNavigating;
        refreshDisplay = true;
      } else if (type == "audio") {
        volumeLevel = doc["v"];
        refreshDisplay = true;
      } else if (type == "notify") {
        String tempSrc = doc["src"] | "";
        if (tempSrc != "Incoming call") {
          notifySrc = tempSrc;
          notifyTitle = doc["title"] | "";
          notifyBody = doc["body"] | "";
          isNotify = true;
          lastNotifyTime = millis();
          refreshDisplay = true;
        }
      } else if (type == "call") {
        String cmd = doc["cmd"] | "";
        if (cmd == "incoming") {
          callName = doc["name"] | "Unknown";
          isIncomingCall = true;
          refreshDisplay = true;
        } else if (cmd == "end" || cmd == "accept") {
          isIncomingCall = false;
          refreshDisplay = true;
        }
      }
    }
  }
}

void sendToGB(String cmd) {
  if (deviceConnected) {
    pCharacteristicTX->setValue(cmd.c_str());
    pCharacteristicTX->notify();
  }
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    for (char c : value) {
      if (c == '\n') {
        processBuffer(inputBuffer);
        inputBuffer = "";
      } else {
        inputBuffer += c;
      }
    }
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    refreshDisplay = true;
    delay(500);
    sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"ESP32-Modern\"})\n");
    sendToGB("GB({\"t\":\"gps\",\"status\":\"ok\"})\n");
    sendToGB("GB({\"t\":\"music\",\"n\":\"info\"})\n");
  }
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    refreshDisplay = true;
    pServer->getAdvertising()->start();
  }
};

// ========================================================
// SETUP & LOOP
// ========================================================
void setup() {
  Serial.begin(115200);  // Serial untuk komunikasi dengan Arduino dan debugging
  // NOTE: Semua komunikasi dengan Arduino menggunakan Serial (USB)
  // Arduino Pro Micro harus terhubung ke pin TX/RX CYD (USB-to-Serial)
  
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(100);
  tft.init();
  tft.setRotation(0);
  
  pinMode(LDR_PIN, INPUT);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] Gagal init SD Card");
  } else {
    Serial.println("[SD] SD Card siap");
    loadTimeFromSD();
    
    // Buat baris baru di trip_log.csv saat boot
    createNewTripLogEntry();
    Serial.println("[SD] Trip log entry created on boot");
  }

  BLEDevice::init("ESP32-Dashboard");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("BLE Ready - Configurable Layout with LDR & Trip Logger");
  updateDisplay();
  
  delay(2000);
  // Kirim konfigurasi awal ke Arduino dengan prefix
  sendConfigToArduino();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Baca data dari Arduino via Serial (USB)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (tripDataBuffer.length() > 0) {
        // Cek apakah data dari Arduino memiliki prefix DATA:
        if (tripDataBuffer.startsWith(DATA_PREFIX)) {
          // Hapus prefix dan proses data
          String jsonData = tripDataBuffer.substring(strlen(DATA_PREFIX));
          processArduinoData(jsonData);
        } else if (tripDataBuffer.startsWith("pong")) {
          Serial.println("[Arduino] Pong received - Connection OK");
        } else {
          // Data tanpa prefix diabaikan (mungkin debugging)
          Serial.print("[Serial Debug] ");
          Serial.println(tripDataBuffer);
        }
        tripDataBuffer = "";
      }
    } else {
      tripDataBuffer += c;
    }
  }
  
  // Update notifikasi engine status setiap 5 detik saat engine ON
  if (currentEngineStatus && (currentMillis - lastTripUpdate >= 5000)) {
    lastTripUpdate = currentMillis;
    // Update tampilan notifikasi dengan durasi terbaru
    if (isNotify && notifyTitle == "Engine STARTED") {
      notifyBody = "Running: " + formatDuration(currentTripDuration);
      refreshDisplay = true;
    }
  }
  
  // Cek perubahan koneksi
  if (deviceConnected != lastConnectionState) {
    lastConnectionState = deviceConnected;
    refreshDisplay = true;
  }
  
  // Update waktu setiap 1 detik
  if (currentMillis - lastClockCheck >= 1000) {
    updateTimeIfNeeded();
    lastClockCheck = currentMillis;
    
    // Kirim ping setiap 30 detik untuk cek koneksi
    static unsigned long lastPing = 0;
    if (currentMillis - lastPing >= 30000) {
      sendPingToArduino();
      lastPing = currentMillis;
    }
  }
  
  // Auto-hide notifikasi setelah 10 detik
  if (isNotify && (currentMillis - lastNotifyTime >= 10000)) {
    isNotify = false;
    refreshDisplay = true;
  }
  
  // Simpan ke SD setiap menit (untuk config waktu)
  if (currentMillis - lastSDWrite >= 60000) {
    saveTimeToSD();
    lastSDWrite = currentMillis;
  }
  
  // Redraw layar jika perlu
  if (refreshDisplay) {
    updateDisplay();
  }
  
  delay(100);
}