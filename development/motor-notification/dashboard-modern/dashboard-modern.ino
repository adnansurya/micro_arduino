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
#define LDR_PIN 34  // Pin LDR pada CYD (GPIO34)

// ========================================================
// Warna Tema (Variabel - bisa diubah saat runtime)
// ========================================================
uint16_t COLOR_CARD_BG = 0x3186;
uint16_t COLOR_BG_BOTTOM = 0x0000;  // <-- TAMBAHKAN INI
uint16_t COLOR_ACCENT = 0x05FF;
uint16_t COLOR_SUCCESS = 0x07E0;
uint16_t COLOR_WARNING = 0xFD20;
uint16_t COLOR_DANGER = 0xF800;
uint16_t COLOR_PRIMARY = 0x05FF;
uint16_t COLOR_GLASS = 0xC618;
uint16_t COLOR_TEXT_PRIMARY = TFT_WHITE;
uint16_t COLOR_TEXT_SECONDARY = 0x7BEF;

// === WARNA MODE GELAP (Dark Mode) ===
const uint16_t COLOR_CARD_BG_DARK = 0x3186;
const uint16_t COLOR_BG_BOTTOM_DARK = 0x0000;  // <-- TAMBAHKAN INI
const uint16_t COLOR_ACCENT_DARK = 0x05FF;
const uint16_t COLOR_PRIMARY_DARK = 0x05FF;
const uint16_t COLOR_GLASS_DARK = 0xC618;
const uint16_t COLOR_TEXT_PRIMARY_DARK = TFT_WHITE;
const uint16_t COLOR_TEXT_SECONDARY_DARK = 0x7BEF;

// === WARNA MODE TERANG (Light Mode) ===
const uint16_t COLOR_CARD_BG_LIGHT = 0xE73C;      // Abu-abu muda
const uint16_t COLOR_BG_BOTTOM_LIGHT = 0xEF5D;    // <-- TAMBAHKAN INI (putih abu-abu)
const uint16_t COLOR_ACCENT_LIGHT = 0x001F;       // Biru gelap
const uint16_t COLOR_PRIMARY_LIGHT = 0x001F;      // Biru gelap
const uint16_t COLOR_GLASS_LIGHT = 0xCE79;        // Abu-abu untuk border
const uint16_t COLOR_TEXT_PRIMARY_LIGHT = TFT_BLACK;
const uint16_t COLOR_TEXT_SECONDARY_LIGHT = 0x528A; // Abu-abu gelap
const uint16_t COLOR_SUCCESS_LIGHT = 0x04A0;  // Hijau gelap (RGB: 0, 124, 0)

// Variabel tema
bool isDarkMode = true;  // Default: mode gelap

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

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ========================================================
// FUNGSI LDR & MANAJEMEN TEMA
// ========================================================

// Update semua warna berdasarkan mode saat ini
void updateThemeColors() {
  if (isDarkMode) {
    // Mode GELAP (malam)
    COLOR_CARD_BG = COLOR_CARD_BG_DARK;
    COLOR_BG_BOTTOM = COLOR_BG_BOTTOM_DARK;
    COLOR_ACCENT = COLOR_ACCENT_DARK;
    COLOR_PRIMARY = COLOR_PRIMARY_DARK;
    COLOR_GLASS = COLOR_GLASS_DARK;
    COLOR_TEXT_PRIMARY = COLOR_TEXT_PRIMARY_DARK;
    COLOR_TEXT_SECONDARY = COLOR_TEXT_SECONDARY_DARK;
    COLOR_SUCCESS = 0x07E0;  // Hijau terang untuk mode gelap
  } else {
    // Mode TERANG (siang)
    COLOR_CARD_BG = COLOR_CARD_BG_LIGHT;
    COLOR_BG_BOTTOM = COLOR_BG_BOTTOM_LIGHT;
    COLOR_ACCENT = COLOR_ACCENT_LIGHT;
    COLOR_PRIMARY = COLOR_PRIMARY_LIGHT;
    COLOR_GLASS = COLOR_GLASS_LIGHT;
    COLOR_TEXT_PRIMARY = COLOR_TEXT_PRIMARY_LIGHT;
    COLOR_TEXT_SECONDARY = COLOR_TEXT_SECONDARY_LIGHT;
    COLOR_SUCCESS = COLOR_SUCCESS_LIGHT;  // Hijau gelap untuk mode terang
  }
}

// Membaca LDR dan menentukan tema (hanya dipanggil saat refreshDisplay = true)
// Logika: jika analogRead(34) == 0 -> TERANG, jika > 0 -> GELAP
void checkLDRAndUpdateTheme() {
  int ldrValue = analogRead(LDR_PIN);
  bool newDarkMode;
  
  // Logika sederhana: 0 = TERANG, >0 = GELAP
  if (ldrValue == 0) {
    newDarkMode = false;  // Mode TERANG (Siang)
    Serial.println("[LDR] Nilai: 0 -> Mode TERANG (Siang)");
  } else {
    newDarkMode = true;   // Mode GELAP (Malam)
    Serial.print("[LDR] Nilai: ");
    Serial.print(ldrValue);
    Serial.println(" -> Mode GELAP (Malam)");
  }
  
  // Jika mode berubah, update warna dan butuh redraw
  if (newDarkMode != isDarkMode) {
    isDarkMode = newDarkMode;
    updateThemeColors();
    Serial.println("[LDR] Tema berubah! ✅");
    refreshDisplay = true;  // Pastikan refresh terjadi
  }
}

// Mendapatkan warna background gradient berdasarkan mode
uint16_t getBgColor(int i, int height) {
  if (isDarkMode) {
    // Mode gelap: gradient biru tua ke hitam
    uint8_t factor = (i * 255) / height;
    uint8_t r = 5 + (factor * 15 / 255);
    uint8_t g = 5 + (factor * 10 / 255);
    uint8_t b = 20 + (factor * 30 / 255);
    return color565(r, g, b);
  } else {
    // Mode terang: gradient putih ke abu-abu muda
    uint8_t factor = (i * 255) / height;
    uint8_t r = 245 - (factor * 20 / 255);
    uint8_t g = 245 - (factor * 20 / 255);
    uint8_t b = 245 - (factor * 20 / 255);
    return color565(r, g, b);
  }
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
  // Di mode terang, border lebih tegas
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
// KOMPONEN UI (MENGGUNAKAN KONSTANTA POSISI)
// ========================================================
void drawModernHeader() {
  // Header gradient
  for (int i = 0; i < HEADER_HEIGHT; i++) {
    if (isDarkMode) {
      tft.drawFastHLine(0, i, tft.width(), color565(10, 10, 30));
    } else {
      tft.drawFastHLine(0, i, tft.width(), color565(240, 240, 245));
    }
  }
  
  // === PERBAIKAN: Gunakan background color yang sama dengan header ===
  // Untuk mode gelap: background header gelap, untuk mode terang: background header terang
  uint16_t bgColor = isDarkMode ? color565(10, 10, 30) : color565(240, 240, 245);
  
  tft.setTextColor(COLOR_TEXT_PRIMARY, bgColor);  // Gunakan bgColor, bukan TFT_TRANSPARENT
  tft.drawCentreString(currentTime, tft.width() / 2, JAM_POS_Y, 6);

  if (lastUnixTime > 0) {
    struct tm *tmp = localtime(&lastUnixTime);
    char dateBuf[50];
    sprintf(dateBuf, "%s, %02d %s %d", hariIndo[tmp->tm_wday], tmp->tm_mday,
            bulanIndo[tmp->tm_mon], tmp->tm_year + 1900);
    tft.setTextColor(COLOR_TEXT_SECONDARY, bgColor);  // Gunakan bgColor
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
  String shortTitle = notifyTitle.length() > 35 ? notifyTitle.substring(0, 32) + "..." : notifyTitle;
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

  if (deviceConnected) {
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
  
  // Ambil warna background di posisi jam standby untuk menghilangkan efek hijau
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
// UPDATE DISPLAY (PENGECEKAN LDR DI SINI)
// ========================================================
void updateDisplay() {
  // ========================================================
  // SEBELUM REDRAW: BACA LDR DAN CEK PERUBAHAN TEMA
  // ========================================================
  checkLDRAndUpdateTheme();
  
  // ========================================================
  // LAKUKAN REDRAW LAYAR
  // ========================================================
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
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(100);
  tft.init();
  tft.setRotation(0);
  
  // Inisialisasi pin LDR
  pinMode(LDR_PIN, INPUT);
  
  if (!SD.begin(SD_CS)) Serial.println("[SD] Gagal init SD Card");
  else {
    Serial.println("[SD] SD Card siap");
    loadTimeFromSD();
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

  Serial.println("BLE Ready - Configurable Layout with LDR");
  updateDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Cek perubahan koneksi
  if (deviceConnected != lastConnectionState) {
    lastConnectionState = deviceConnected;
    refreshDisplay = true;
  }
  
  // Update waktu setiap 1 detik
  if (currentMillis - lastClockCheck >= 1000) {
    updateTimeIfNeeded();
    lastClockCheck = currentMillis;
  }
  
  // Auto-hide notifikasi setelah 5 menit
  if (isNotify && (currentMillis - lastNotifyTime >= 300000)) {
    isNotify = false;
    refreshDisplay = true;
  }
  
  // Simpan ke SD setiap menit
  if (currentMillis - lastSDWrite >= 60000) {
    saveTimeToSD();
    lastSDWrite = currentMillis;
  }
  
  // Redraw layar jika perlu (LDR akan dicek di dalam updateDisplay)
  if (refreshDisplay) {
    updateDisplay();
  }
  
  delay(100);
}