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
#include <math.h>

TFT_eSPI tft = TFT_eSPI();

// ========================================================
// =============== STATE MANAGEMENT HALAMAN ===============
// ========================================================
enum PageState {
  PAGE_MAIN,
  PAGE_LAST_TRIP
};
PageState currentPageState = PAGE_MAIN;

// Struktur data untuk menampung riwayat trip
struct TripRecord {
  String waktu;
  String jarak;
  String avgSpeed;
  String durasi;
  bool valid = false;
};
TripRecord lastTrips[3]; // Menampung maksimal 3 trip terakhir

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

// === CARD MUSIK & TRIP COMPUTER (POSISI DISAMAKAN) ===
#define MUSIC_CARD_HEIGHT 95         
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
// Warna Tema (Variabel - bisa diubah saat runtime)
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

// === WARNA MODE GELAP (Dark Mode) ===
const uint16_t COLOR_CARD_BG_DARK = 0x3186;
const uint16_t COLOR_BG_BOTTOM_DARK = 0x0000;
const uint16_t COLOR_ACCENT_DARK = 0x05FF;
const uint16_t COLOR_PRIMARY_DARK = 0x05FF;
const uint16_t COLOR_GLASS_DARK = 0xC618;
const uint16_t COLOR_TEXT_PRIMARY_DARK = TFT_WHITE;
const uint16_t COLOR_TEXT_SECONDARY_DARK = 0x7BEF;

// === WARNA MODE TERANG (Light Mode) ===
const uint16_t COLOR_CARD_BG_LIGHT = 0xE73C;      
const uint16_t COLOR_BG_BOTTOM_LIGHT = 0xEF5D;    
const uint16_t COLOR_ACCENT_LIGHT = 0x001F;       
const uint16_t COLOR_PRIMARY_LIGHT = 0x001F;      
const uint16_t COLOR_GLASS_LIGHT = 0xCE79;        
const uint16_t COLOR_TEXT_PRIMARY_LIGHT = TFT_BLACK;
const uint16_t COLOR_TEXT_SECONDARY_LIGHT = 0x528A; 
const uint16_t COLOR_SUCCESS_LIGHT = 0x04A0;  

// Variabel tema (Default dimulai dari mode gelap)
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

// Variabel tracking UI lama
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

// Variabel UI Popup
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
// VARIABLES FOR GPS LOGGING & TRIP METRICS
// ========================================================
String lat = "0.0";
String lng = "0.0";
double lastLat = 0.0;
double lastLng = 0.0;
double totalJarakTempuh = 0.0;
bool isFirstGPSLog = true;

unsigned long posisiBarisTerakhir = 0; 
String waktuAwalTrip = "--:--";
String koordinatAwalTrip = "-";

// Variabel Sakelar Tampilan Trip Meter via Serial
bool showTripMeter = false; 
String serialInputStr = "";

// ========================================================
// FUNGSI BANTU POSISI & UTILITY
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

int getVolumeY() { return tft.height() - VOLUME_Y_FROM_BOTTOM; }
int getFooterY() { return tft.height() - FOOTER_Y_FROM_BOTTOM; }

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
// FUNGSI TRIP KALKULASI & SD LOGGING
// ========================================================

String hitungWaktuTempuhStr() {
  unsigned long totalDetik = millis() / 1000;
  int jam = totalDetik / 3600;
  int menit = (totalDetik % 3600) / 60;
  int detik = totalDetik % 60;
  
  char buf[30];
  if (jam > 0) sprintf(buf, "%dj %dm", jam, menit); 
  else if (menit > 0) sprintf(buf, "%dm %ds", menit, detik);
  else sprintf(buf, "%ds", detik);
  return String(buf);
}

double hitungJarakHaversine(double lat1, double lon1, double lat2, double lon2) {
  double R = 6371.0; 
  double dLat = (lat2 - lat1) * M_PI / 180.0;
  double dLon = (lon2 - lon1) * M_PI / 180.0;
  double rLat1 = lat1 * M_PI / 180.0;
  double rLat2 = lat2 * M_PI / 180.0;
  
  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(rLat1) * cos(rLat2) *
             sin(dLon / 2) * sin(dLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

void updateCSVLastRow(bool isBarisBaru) {
  File file = SD.open("/trip_log.csv", FILE_WRITE);
  if (!file) {
    Serial.println("[SD] Gagal membuka trip_log.csv!");
    return;
  }

  if (file.size() == 0) {
    file.println("Waktu Awal,Waktu Akhir,Koordinat Awal,Koordinat Akhir,Waktu Tempuh,Jarak Tempuh,Rata-rata Kecepatan");
  }

  if (isBarisBaru) {
    file.seek(file.size()); 
    posisiBarisTerakhir = file.position(); 
  } else {
    file.seek(posisiBarisTerakhir);
  }

  double waktuJam = (double)millis() / 3600000.0; 
  double rataRataKecepatan = 0.0;
  if (waktuJam > 0.0) {
    rataRataKecepatan = totalJarakTempuh / waktuJam;
  }

  String dataBaris = waktuAwalTrip + "," + 
                     currentTime + " (RTC)," + 
                     koordinatAwalTrip + "," + 
                     (lat + ";" + lng) + "," + 
                     hitungWaktuTempuhStr() + "," + 
                     String(totalJarakTempuh, 2) + " Km," +
                     String(rataRataKecepatan, 1) + " Km/h                     \n";

  file.print(dataBaris);
  file.close();
  Serial.println("[SD] trip_log.csv berhasil diperbarui.");

  time_t now;
  time(&now);
  if (now > 1600000000) { 
    File configFile = SD.open("/config.txt", FILE_WRITE);
    if (configFile) {
      configFile.println(now);
      configFile.close();
      Serial.println("[SD-Backup] Waktu sistem dicadangkan via GPS Trigger Event.");
    }
  }
}

// ========================================================
// FUNGSI MEMBACA DATA HISTORI TRIP DARI SD (3 TERAKHIR)
// ========================================================
void readLast3TripsFromSD() {
  // Reset data lama
  for (int i = 0; i < 3; i++) lastTrips[i].valid = false;

  File file = SD.open("/trip_log.csv", FILE_READ);
  if (!file) {
    Serial.println("[SD] Gagal membaca log untuk riwayat trip!");
    return;
  }

  // Karena file CSV dibaca linear, kita tampung baris-barisnya 
  // Untuk data besar, disarankan membatasi buffer string
  String lines[50]; 
  int lineCount = 0;

  // Lewati header csv
  if (file.available()) file.readStringUntil('\n');

  while (file.available() && lineCount < 50) {
    String l = file.readStringUntil('\n');
    l.trim();
    if (l.length() > 10) {
      lines[lineCount] = l;
      lineCount++;
    }
  }
  file.close();

  // Ambil 3 data paling terakhir dari array buffer lines
  int slot = 0;
  for (int i = lineCount - 1; i >= 0 && slot < 3; i--) {
    String currentLine = lines[i];
    
    // Parsing manual split berbasis tanda koma (CSV)
    // Format: Waktu Awal, Waktu Akhir, Koor Awal, Koor Akhir, Waktu Tempuh, Jarak Tempuh, Rata-rata Kecepatan
    int indices[7];
    int idxCount = 0;
    int searchPos = 0;
    
    while ((searchPos = currentLine.indexOf(',', searchPos)) != -1 && idxCount < 7) {
      indices[idxCount++] = searchPos;
      searchPos++;
    }

    if (idxCount >= 6) {
      lastTrips[slot].waktu    = currentLine.substring(0, indices[0]);
      lastTrips[slot].durasi   = currentLine.substring(indices[3] + 1, indices[4]);
      lastTrips[slot].jarak    = currentLine.substring(indices[4] + 1, indices[5]);
      lastTrips[slot].avgSpeed = currentLine.substring(indices[5] + 1);
      lastTrips[slot].valid    = true;
      slot++;
    }
  }
  Serial.print("[SD] Berhasil memuat data historis trip. Jumlah: "); Serial.println(slot);
}

// ========================================================
// MANAJEMEN TEMA MANUAL (SET TIAP KALI TRIGER TOUCH)
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
// MANAJEMEN SD CARD KONDISI AWAL SYSTEM
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
// EFEK VISUAL & KOMPONEN UI GRAPHICS
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

void drawModernHeader() {
  for (int i = 0; i < HEADER_HEIGHT; i++) {
    if (isDarkMode) tft.drawFastHLine(0, i, tft.width(), color565(10, 10, 30));
    else tft.drawFastHLine(0, i, tft.width(), color565(240, 240, 245));
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

void drawMinimalistTripMeter() {
  int targetY = getMusicCardY(); 
  
  drawGlassEffect(10, targetY, tft.width() - 20, MUSIC_CARD_HEIGHT, 10);
  drawBorderWithColor(10, targetY, tft.width() - 20, MUSIC_CARD_HEIGHT, 10, COLOR_ACCENT);
  
  // === KOLOM KIRI: JARAK TEMPUH ===
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawString("DIST", 20, targetY + 12, 1);
  tft.setTextColor(COLOR_SUCCESS, COLOR_CARD_BG);
  tft.drawString(String(totalJarakTempuh, 1), 20, targetY + 28, 4); 
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  tft.drawString("Km", 20, targetY + 68, 2);

  // === KOLOM TENGAH: AVG SPEED ===
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawCentreString("AVG SPD", tft.width() / 2, targetY + 12, 1);
  tft.setTextColor(COLOR_WARNING, COLOR_CARD_BG);
  double waktuJam = (double)millis() / 3600000.0;
  double speedAvg = waktuJam > 0 ? (totalJarakTempuh / waktuJam) : 0.0;
  tft.drawCentreString(String(speedAvg, 0), tft.width() / 2, targetY + 28, 4); 
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  tft.drawCentreString("Km/h", tft.width() / 2, targetY + 68, 2);

  // === KOLOM KANAN: WAKTU TEMPUH ===
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
  tft.drawRightString("TIME", tft.width() - 20, targetY + 12, 1);
  tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
  tft.drawRightString(hitungWaktuTempuhStr(), tft.width() - 20, targetY + 38, 2);
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
          if (remaining2.length() > maxChars - 3) line3 = remaining2.substring(0, maxChars - 3) + "...";
          else line3 = remaining2;
        }
      } else line2 = remaining;
    } else line1 = bodyText;

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
  tft.drawRightString("Dashboard v2.5", tft.width() - 20, footerY, 1);
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
// RENDERING FULL SCREEN PAGE: LAST TRIP HISTORY
// ========================================================
void drawLastTripPage() {
  drawGradientBackground();
  
  // Perbaikan: tft.setFillColor dihapus karena tidak ada di library TFT_eSPI
  tft.setTextColor(COLOR_PRIMARY, COLOR_BG_BOTTOM);
  tft.drawCentreString("RIWAYAT PERJALANAN", tft.width() / 2, 12, 2);
  tft.drawRect(20, 32, tft.width() - 40, 2, COLOR_ACCENT);

  int startY = 45;
  int cardH = 55;
  int spacing = 62;

  for (int i = 0; i < 3; i++) {
    int currentY = startY + (i * spacing);
    drawGlassEffect(10, currentY, tft.width() - 20, cardH, 8);
    
    if (lastTrips[i].valid) {
      drawBorderWithColor(10, currentY, tft.width() - 20, cardH, 8, COLOR_GLASS);
      
      // Baris Atas Card: Label ID & Waktu Record
      tft.setTextColor(COLOR_PRIMARY, COLOR_CARD_BG);
      tft.drawString("#" + String(i + 1), 20, currentY + 8, 2);
      tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
      tft.drawString(lastTrips[i].waktu, 50, currentY + 8, 1);

      // Baris Bawah Card: Metrik Data Trip
      tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
      tft.drawString("Dist: ", 20, currentY + 28, 1);
      tft.setTextColor(COLOR_SUCCESS, COLOR_CARD_BG);
      tft.drawString(lastTrips[i].jarak, 55, currentY + 28, 1);

      tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
      tft.drawString("Time: ", 120, currentY + 28, 1);
      tft.setTextColor(COLOR_TEXT_PRIMARY, COLOR_CARD_BG);
      tft.drawString(lastTrips[i].durasi, 155, currentY + 28, 1);

      tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
      tft.drawString("Avg: ", 215, currentY + 28, 1);
      tft.setTextColor(COLOR_WARNING, COLOR_CARD_BG);
      tft.drawString(lastTrips[i].avgSpeed, 245, currentY + 28, 1);
    } else {
      // Jika data kosong / belum ada log di CSV
      drawBorderWithColor(10, currentY, tft.width() - 20, cardH, 8, 0x4208);
      tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_CARD_BG);
      tft.drawCentreString("- Belum Ada Data Trip -", tft.width() / 2, currentY + 20, 1);
    }
  }

  // Petunjuk Navigasi di bagian bawah full-screen
  tft.setTextColor(COLOR_TEXT_SECONDARY, COLOR_BG_BOTTOM);
  tft.drawCentreString("Ketik SINGLE_TOUCH untuk kembali", tft.width() / 2, tft.height() - 22, 1);
}

void updateDisplay() {
  if (currentPageState == PAGE_LAST_TRIP) {
    drawLastTripPage();
  } else {
    if (!deviceConnected) {
      drawStandbyMode();
    } else {
      drawGradientBackground();
      drawModernHeader();
      
      // LAYER ATAS (NAVIGASI & POPUP NOTIFIKASI STANDARD)
      if (isNavigating) drawNavigationCard();
      else if (isNotify) drawNotificationPopup();
      
      // LAYER BAWAH (LOGIKA SAKELAR SUB-HALAMAN UTAMA)
      if (isIncomingCall) {
        drawCallCard(); 
      } else {
        if (showTripMeter) {
          drawMinimalistTripMeter(); 
        } else {
          drawMusicPlayer();         
        }
      }
      
      drawVolumeBar();
      drawFooter();
    }
  }
  refreshDisplay = false;
}

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
// BLE & CORE DATA INCOMING PROCESSING
// ========================================================
void processBuffer(String data) {
  Serial.println("\n[PARSING] " + data);
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    lastUnixTime = data.substring(startIdx, endIdx).toInt();
    if (lastUnixTime > 0) {
      lastUnixTime += 28800; // Offset Zona Waktu WITA
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
      lastSDWrite = millis(); 
      
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
        String tempTitle = doc["title"] | "";
        
        if (tempSrc == "MacroDroid" && tempTitle == "[M-GPS]") {
          String gpsBody = doc["body"] | "";
          if (gpsBody.length() > 0 && gpsBody.indexOf(',') != -1) {
            int commaIndex = gpsBody.indexOf(',');
            lat = gpsBody.substring(0, commaIndex);
            lng = gpsBody.substring(commaIndex + 1);
            lat.trim(); lng.trim();
            
            double currentLat = lat.toDouble();
            double currentLng = lng.toDouble();
            
            if (isFirstGPSLog) {
              lastLat = currentLat;
              lastLng = currentLng;
              koordinatAwalTrip = lat + ";" + lng;
              waktuAwalTrip = currentTime + " (RTC)";
              isFirstGPSLog = false;
              updateCSVLastRow(true); 
            } else {
              double selisihJarak = hitungJarakHaversine(lastLat, lastLng, currentLat, currentLng);
              if (selisihJarak > 0.005) { 
                totalJarakTempuh += selisihJarak;
                lastLat = currentLat;
                lastLng = currentLng;
              }
              updateCSVLastRow(false); 
            }
            
            double totalJam = (double)millis() / 3600000.0;
            double speedAvg = totalJam > 0 ? (totalJarakTempuh / totalJam) : 0.0;
            Serial.print("[TRIP LOG] Jarak: "); Serial.print(totalJarakTempuh, 2); Serial.print(" Km | ");
            Serial.print("Kecepatan Rata-rata: "); Serial.print(speedAvg, 1); Serial.println(" Km/h");
            
            if (showTripMeter && currentPageState == PAGE_MAIN) refreshDisplay = true; 
            
            lastSDWrite = millis(); 
          }
        } 
        else if (tempSrc != "Incoming call") {
          notifySrc = tempSrc;
          notifyTitle = tempTitle;
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
// SETUP & LOOP SYSTEM BINDING
// ========================================================
void setup() {
  Serial.begin(115200);
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(100);
  tft.init();
  tft.setRotation(0);
  
  updateThemeColors(); 

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

  Serial.println("BLE Dashboard Ready - Speed Tracking Log Active");
  updateDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // === INTERSEPSI INPUT SERIAL (SINGLE, LONG, DOUBLE, & TRIPLE TOUCH) ===
  while (Serial.available() > 0) {
    char rc = Serial.read();
    if (rc == '\n' || rc == '\r') {
      serialInputStr.trim(); 
      
      if (serialInputStr == "SINGLE_TOUCH") {
        if (currentPageState == PAGE_LAST_TRIP) {
          currentPageState = PAGE_MAIN; // Kembali dari halaman histori ke utama
          refreshDisplay = true;
          Serial.println("[UI] Kembali ke Halaman Utama dari Last Trip View.");
        } else {
          // Aksi standard halaman utama
          showTripMeter = !showTripMeter; 
          refreshDisplay = true;          
          Serial.print("[UI] Switch Halaman Musik -> Trip Meter: ");
          Serial.println(showTripMeter ? "ON" : "OFF");
        }
      } 
      else if (serialInputStr == "LONG_TOUCH") {
        if (isNotify) {
          isNotify = false;         
          notifySrc = "";           
          notifyTitle = "";
          notifyBody = "";
          refreshDisplay = true;    
          Serial.println("[UI] Notifikasi dihapus paksa via LONG_TOUCH.");
        } else {
          Serial.println("[UI] Perintah LONG_TOUCH diabaikan karena tidak ada notifikasi.");
        }
      }
      else if (serialInputStr == "DOUBLE_TOUCH") {
        isDarkMode = !isDarkMode;       
        updateThemeColors();           
        refreshDisplay = true;         
        Serial.print("[UI] Tema diubah secara manual via DOUBLE_TOUCH. Dark Mode: ");
        Serial.println(isDarkMode ? "AKTIF" : "NONAKTIF");
      }
      else if (serialInputStr == "TRIPLE_TOUCH") {
        if (currentPageState != PAGE_LAST_TRIP) {
          readLast3TripsFromSD();        // Tarik data 3 baris terbawah dari log csv
          currentPageState = PAGE_LAST_TRIP; // Pindah ke halaman full screen
          refreshDisplay = true;
          Serial.println("[UI] Membuka Halaman Riwayat Perjalanan (Last Trip).");
        }
      }
      
      serialInputStr = ""; 
    } else {
      serialInputStr += rc;
    }
  }

  if (deviceConnected != lastConnectionState) {
    lastConnectionState = deviceConnected;
    refreshDisplay = true;
  }
  
  if (currentMillis - lastClockCheck >= 1000) {
    updateTimeIfNeeded();
    lastClockCheck = currentMillis;
  }
  
  if (isNotify && (currentMillis - lastNotifyTime >= 300000)) {
    isNotify = false;
    refreshDisplay = true;
  }
  
  if (currentMillis - lastSDWrite >= 60000) {
    saveTimeToSD();
    lastSDWrite = currentMillis;
    Serial.println("[SD-Backup] Backup waktu berkala di loop dijalankan.");
  }
  
  if (refreshDisplay) {
    updateDisplay();
  }
  
  delay(100);
}