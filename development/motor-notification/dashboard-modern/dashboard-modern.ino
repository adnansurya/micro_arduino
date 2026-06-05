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
// KONFIGURASI TEMA MODERN - DARK GLASS PREMIUM
// ========================================================
// Warna Dasar
#define COLOR_BG_TOP      0x2104
#define COLOR_BG_BOTTOM   0x0000
#define COLOR_CARD_BG     0x3186

// Warna Aksen Dinamis
uint16_t COLOR_ACCENT = 0x05FF;
uint16_t COLOR_SUCCESS = 0x07E0;
uint16_t COLOR_WARNING = 0xFD20;
uint16_t COLOR_DANGER = 0xF800;
uint16_t COLOR_PRIMARY = 0x05FF;
uint16_t COLOR_GLASS = 0xC618;

// Warna Sekunder
uint16_t COLOR_TEXT_PRIMARY = TFT_WHITE;
uint16_t COLOR_TEXT_SECONDARY = 0x7BEF;

// BLE UUID
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

// Variabel untuk tracking perubahan data
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
unsigned long lastNotifyMillis = 0;

// Timer untuk update jam (hanya cek perubahan menit)
unsigned long lastClockCheck = 0;
unsigned long lastSDWrite = 0;
unsigned long lastProgressUpdate = 0;

const char *hariIndo[] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
const char *bulanIndo[] = { "Januari", "Februari", "Maret", "April", "Mei", "Juni",
                            "Juli", "Agustus", "September", "Oktober", "November", "Desember" };

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

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ========================================================
// CEK PERUBAHAN DATA (HANYA UPDATE JIKA BERUBAH)
// ========================================================
bool hasDataChanged() {
  bool changed = false;
  
  // Cek perubahan waktu (menit)
  if (lastCurrentTime != currentTime) {
    lastCurrentTime = currentTime;
    changed = true;
  }
  
  // Cek perubahan data musik
  if (lastTitle != title || lastArtist != artist || lastMusicState != musicState) {
    lastTitle = title;
    lastArtist = artist;
    lastMusicState = musicState;
    changed = true;
  }
  
  // Cek perubahan posisi lagu (setiap detik saat play)
  if (musicState == "play" && lastPositionSec != positionSec) {
    lastPositionSec = positionSec;
    changed = true;
  }
  
  // Cek perubahan navigasi
  if (lastNavInstr != navInstr || lastNavDist != navDist || lastNavEta != navEta || lastIsNavigating != isNavigating) {
    lastNavInstr = navInstr;
    lastNavDist = navDist;
    lastNavEta = navEta;
    lastIsNavigating = isNavigating;
    changed = true;
  }
  
  // Cek perubahan volume
  if (lastVolumeLevel != volumeLevel) {
    lastVolumeLevel = volumeLevel;
    changed = true;
  }
  
  // Cek perubahan notifikasi
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
  
  // Cek perubahan panggilan
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
// MANAJEMEN SD CARD & WAKTU
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
// EFEK VISUAL STATIS
// ========================================================
void drawGradientBackground() {
  for (int i = 0; i < tft.height(); i++) {
    uint8_t factor = (i * 255) / tft.height();
    uint8_t r = 5 + (factor * 15 / 255);
    uint8_t g = 5 + (factor * 10 / 255);
    uint8_t b = 20 + (factor * 30 / 255);
    tft.drawFastHLine(0, i, tft.width(), color565(r, g, b));
  }
}

void drawGlassEffect(int x, int y, int w, int h, int radius = 12) {
  tft.fillRoundRect(x, y, w, h, radius, COLOR_CARD_BG);
  tft.drawRoundRect(x, y, w, h, radius, COLOR_GLASS);
}

void drawNeonProgressBar(int x, int y, int width, int height, float progress, uint16_t color) {
  tft.fillRoundRect(x, y, width, height, height/2, 0x2104);
  tft.drawRoundRect(x, y, width, height, height/2, color);
  
  int fillWidth = width * progress;
  if(fillWidth > 0) {
    tft.fillRoundRect(x, y, fillWidth, height, height/2, color);
  }
}

// ========================================================
// KOMPONEN UI UTAMA
// ========================================================
void drawModernHeader() {
  // Header gradient
  for(int i = 0; i < 55; i++) {
    tft.drawFastHLine(0, i, tft.width(), color565(10, 10, 30));
  }
  
  // Jam digital
  tft.setTextColor(COLOR_TEXT_PRIMARY, TFT_TRANSPARENT);
  tft.drawCentreString(currentTime, tft.width() / 2, 8, 6);
  
  // Tanggal
  if(lastUnixTime > 0) {
    struct tm *tmp = localtime(&lastUnixTime);
    char dateBuf[50];
    sprintf(dateBuf, "%s, %02d %s %d", hariIndo[tmp->tm_wday], tmp->tm_mday, 
            bulanIndo[tmp->tm_mon], tmp->tm_year + 1900);
    tft.setTextColor(COLOR_TEXT_SECONDARY, TFT_TRANSPARENT);
    tft.drawCentreString(dateBuf, tft.width() / 2, 58, 2);
  }
  
  // Garis aksen
  tft.drawRect(20, 75, tft.width() - 40, 2, COLOR_ACCENT);
}

void drawNavigationCard() {
  int cardY = 90;
  int cardHeight = 90;
  
  drawGlassEffect(10, cardY, tft.width() - 20, cardHeight, 12);
  
  tft.setTextColor(COLOR_PRIMARY, COLOR_CARD_BG);
  tft.drawCentreString("NAVIGASI", tft.width() / 2, cardY + 10, 2);
  
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  String displayInstr = navInstr;
  if(displayInstr.length() > 25) {
    displayInstr = displayInstr.substring(0, 22) + "...";
  }
  tft.drawCentreString(displayInstr, tft.width() / 2, cardY + 35, 2);
  
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawString(" " + navDist, 20, cardY + 60, 1);
  tft.drawRightString(navEta + " ", tft.width() - 20, cardY + 60, 1);
}

void drawMusicPlayer() {
  int musicY = (isNavigating || isNotify) ? 190 : 110;
  int cardHeight = 105;
  
  drawGlassEffect(10, musicY, tft.width() - 20, cardHeight, 12);
  
  // Simulasi album art statis
  for(int i = 0; i < 3; i++) {
    int radius = 25 - (i * 6);
    uint16_t color = (musicState == "play") ? color565(0, 140, 60) : color565(140, 60, 0);
    tft.fillCircle(45, musicY + 40, radius, color);
  }
  tft.fillCircle(45, musicY + 40, 18, COLOR_CARD_BG);
  
  // Judul lagu
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  String displayTitle = title;
  if(displayTitle.length() > 22) {
    displayTitle = displayTitle.substring(0, 19) + "...";
  }
  tft.drawString(displayTitle, 75, musicY + 15, 2);
  
  // Nama artis
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  String displayArtist = artist.length() > 25 ? artist.substring(0, 22) + ".." : artist;
  tft.drawString(displayArtist, 75, musicY + 40, 1);
  
  // Progress bar
  float progress = (durationSec > 0) ? (float)positionSec / durationSec : 0;
  drawNeonProgressBar(75, musicY + 58, tft.width() - 100, 6, progress, 
                     (musicState == "play") ? COLOR_SUCCESS : COLOR_WARNING);
  
  // Waktu
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawString(formatTime(positionSec) + " / " + formatTime(durationSec), 75, musicY + 72, 1);
  
  // Status icon
  String statusIcon = (musicState == "play") ? "NOW PLAYING" : "PAUSED";
  tft.setTextColor((musicState == "play") ? COLOR_SUCCESS : COLOR_WARNING, COLOR_CARD_BG);
  tft.drawString(statusIcon, 75, musicY + 92, 1);
}

void drawCallCard() {
  int callY = (isNavigating || isNotify) ? 190 : 110;
  
  drawGlassEffect(10, callY, tft.width() - 20, 85, 12);
  tft.drawRoundRect(10, callY, tft.width() - 20, 85, 12, COLOR_DANGER);
  
  tft.setTextColor(COLOR_DANGER, COLOR_CARD_BG);
  tft.drawCentreString("PANGGILAN MASUK", tft.width() / 2, callY + 15, 2);
  
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  tft.drawCentreString(callName, tft.width() / 2, callY + 45, 2);
  
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawCentreString("Angkat di HP Anda", tft.width() / 2, callY + 70, 1);
}

void drawNotificationPopup() {
  if(!isNotify) return;
  
  int notifY = 85;
  
  drawGlassEffect(10, notifY, tft.width() - 20, 65, 10);
  tft.fillRoundRect(10, notifY, tft.width() - 20, 65, 10, COLOR_DANGER);
  tft.fillRoundRect(12, notifY + 2, tft.width() - 24, 61, 8, COLOR_CARD_BG);
  
  tft.setTextColor(COLOR_DANGER, COLOR_CARD_BG);
  tft.drawString(notifySrc, 20, notifY + 12, 1);
  
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  String shortTitle = notifyTitle.length() > 22 ? notifyTitle.substring(0, 19) + "..." : notifyTitle;
  tft.drawString(shortTitle, 20, notifY + 32, 2);
  
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  String shortBody = notifyBody.length() > 28 ? notifyBody.substring(0, 25) + "..." : notifyBody;
  tft.drawString(shortBody, 20, notifY + 52, 1);
}

void drawVolumeBar() {
  int volY = tft.height() - 50;
  
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG_BOTTOM);
  tft.drawCentreString("VOLUME", tft.width() / 2, volY, 1);
  
  float volumeProgress = volumeLevel / 100.0;
  drawNeonProgressBar((tft.width() - 180) / 2, volY + 18, 180, 6, volumeProgress, COLOR_PRIMARY);
  
  tft.setTextColor(COLOR_PRIMARY, COLOR_BG_BOTTOM);
  tft.drawCentreString(String(volumeLevel) + "%", tft.width() / 2, volY + 32, 1);
}

void drawFooter() {
  int footerY = tft.height() - 18;
  
  tft.drawFastHLine(20, footerY - 5, tft.width() - 40, COLOR_TEXT_SECONDARY);
  
  if(deviceConnected) {
    tft.setTextColor(COLOR_SUCCESS, COLOR_BG_BOTTOM);
    tft.drawString("Connected", 20, footerY, 1);
  } else {
    tft.setTextColor(COLOR_DANGER, COLOR_BG_BOTTOM);
    tft.drawString("Disconnected", 20, footerY, 1);
  }
  
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG_BOTTOM);
  tft.drawRightString("Dashboard v2.0", tft.width() - 20, footerY, 1);
}

void drawStandbyMode() {
  drawGradientBackground();
  
  tft.setTextColor(COLOR_TEXT_PRIMARY, TFT_TRANSPARENT);
  tft.drawCentreString(currentTime, tft.width() / 2, 70, 6);
  
  if(lastUnixTime > 0) {
    struct tm *tmp = localtime(&lastUnixTime);
    char dateBuf[50];
    sprintf(dateBuf, "%s, %02d %s %d", hariIndo[tmp->tm_wday], tmp->tm_mday, 
            bulanIndo[tmp->tm_mon], tmp->tm_year + 1900);
    tft.setTextColor(COLOR_TEXT_SECONDARY, TFT_TRANSPARENT);
    tft.drawCentreString(dateBuf, tft.width() / 2, 130, 2);
  }
  
  tft.setTextColor(COLOR_WARNING, COLOR_BG_BOTTOM);
  tft.drawCentreString("MENUNGGU KONEKSI", tft.width() / 2, 200, 2);
  
  drawFooter();
}

// ========================================================
// UPDATE DISPLAY (HANYA SAAT refreshDisplay = true)
// ========================================================
void updateDisplay() {
  if (!deviceConnected) {
    drawStandbyMode();
  } else {
    drawGradientBackground();
    drawModernHeader();
    
    if (isNavigating) {
      drawNavigationCard();
    } else if (isNotify) {
      drawNotificationPopup();
    }
    
    if (isIncomingCall) {
      drawCallCard();
    } else {
      drawMusicPlayer();
    }
    
    drawVolumeBar();
    drawFooter();
  }
  
  refreshDisplay = false;
}

// ========================================================
// UPDATE WAKTU (CEK PERUBAHAN MENIT)
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
    
    // Hanya update jika menit berubah
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
      lastUnixTime += 28800; // WITA
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
      } 
      else if (type == "musicstate") {
        musicState = doc["state"].as<String>();
        positionSec = doc["position"] | 0;
        refreshDisplay = true;
      } 
      else if (type == "nav") {
        navInstr = doc["instr"] | "";
        String rawDist = doc["distance"] | "";
        if (rawDist.length() >= 2) {
          rawDist.setCharAt(rawDist.length() - 2, ' ');
        }
        navDist = rawDist;
        navEta = doc["eta"] | "";
        isNavigating = (navInstr != "" && navInstr != " " && navInstr != "null");
        refreshDisplay = true;
      } 
      else if (type == "audio") {
        volumeLevel = doc["v"];
        refreshDisplay = true;
      } 
      else if (type == "notify") {
        String tempSrc = doc["src"] | "";
        if (tempSrc != "Incoming call") {
          notifySrc = tempSrc;
          notifyTitle = doc["title"] | "";
          notifyBody = doc["body"] | "";
          isNotify = true;
          refreshDisplay = true;
          lastNotifyMillis = millis();
        }
      } 
      else if (type == "call") {
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

// ========================================================
// BLE CALLBACKS
// ========================================================
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
  Serial.begin(115200);
  
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(100);
  
  tft.init();
  tft.setRotation(0);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] Gagal init SD Card");
  } else {
    Serial.println("[SD] SD Card siap");
    loadTimeFromSD();
  }
  
  BLEDevice::init("ESP32-Dashboard");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX, 
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristicTX->addDescriptor(new BLE2902());
  
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX, 
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  
  pService->start();
  pServer->getAdvertising()->start();
  
  Serial.println("BLE Ready - ESP32 Modern Dashboard (Optimized Updates)");
  updateDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Cek perubahan koneksi
  if (deviceConnected != lastConnectionState) {
    lastConnectionState = deviceConnected;
    refreshDisplay = true;
  }
  
  // Update waktu setiap 1 detik (untuk cek perubahan menit)
  if (currentMillis - lastClockCheck >= 1000) {
    updateTimeIfNeeded();
    lastClockCheck = currentMillis;
  }
  
  // Update progress bar musik setiap detik (hanya saat play)
  if (deviceConnected && musicState == "play" && positionSec < durationSec) {
    if (currentMillis - lastProgressUpdate >= 1000) {
      positionSec++;
      refreshDisplay = true;
      lastProgressUpdate = currentMillis;
    }
  }
  
  // Auto hide notifikasi setelah 5 detik
  if (isNotify && (currentMillis - lastNotifyMillis > 5000)) {
    isNotify = false;
    refreshDisplay = true;
  }
  
  // Simpan ke SD setiap menit
  if (currentMillis - lastSDWrite >= 60000) {
    saveTimeToSD();
    lastSDWrite = currentMillis;
  }
  
  // Update layar HANYA jika ada perubahan data
  if (refreshDisplay) {
    updateDisplay();
  }
  
  delay(100); // Delay lebih panjang, layar hanya update saat perlu
}