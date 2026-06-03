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

// --- VARIABEL WARNA (TEMA PUTIH ADOPSI BARU) ---
uint16_t COLOR_BG = TFT_WHITE;
uint16_t COLOR_TEXT = TFT_BLACK;
uint16_t COLOR_ACCENT = TFT_BLUE;
uint16_t COLOR_NAV_BG = 0xE71C;
uint16_t COLOR_NAV_TXT = 0x0010;
uint16_t COLOR_TITLE = 0x03E0;
uint16_t COLOR_ARTIST = 0x4208;
uint16_t COLOR_STATUS = TFT_MAGENTA;
uint16_t COLOR_VOL = 0x001F;
uint16_t COLOR_BAR_BG = 0xD6BA;
uint16_t COLOR_BAR_FILL = 0x07FF;
uint16_t COLOR_DATE = 0x7BEF;

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define SD_CS 5  // Pin CS MicroSD CYD

BLECharacteristic *pCharacteristicTX;
bool deviceConnected = false;
bool lastConnectionState = false;
String artist = "-", title = "-", currentTime = "--:--";
String navInstr = "", navDist = "", navEta = "";
String musicState = "pause";
int volumeLevel = 0;
long positionSec = 0, durationSec = 0;
time_t lastUnixTime = 0;  // Timestamp global RTC
String inputBuffer = "";
bool refreshDisplay = true;
bool isNavigating = false;

unsigned long lastClockUpdate = 0;
unsigned long lastSDWrite = 0;

const char *hariIndo[] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
const char *bulanIndo[] = { "Januari", "Februari", "Maret", "April", "Mei", "Juni",
                            "Juli", "Agustus", "September", "Oktober", "November", "Desember" };

// --- VARIABEL TAMBAHAN UNTUK OVERLAY NOTIFIKASI & CALL ---
bool isNotify = false;
String notifySrc = "";
String notifyTitle = "";
String notifyBody = "";

bool isIncomingCall = false;
String callName = "";

unsigned long lastNotifyMillis = 0;

String formatTime(long seconds) {
  if (seconds < 0) seconds = 0;
  int m = seconds / 60;
  int s = seconds % 60;
  char buf[10];
  sprintf(buf, "%02d:%02d", m, s);
  return String(buf);
}

// ========================================================
// MANAGEMENT FILE STORAGE (SD CARD)
// ========================================================
void loadTimeFromSD() {
  File file = SD.open("/config.txt", FILE_READ);
  if (!file) {
    Serial.println("[SD] File config.txt tidak ditemukan.");
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
    Serial.println("[SD] Sukses memuat RTC Backup dari MicroSD.");
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
      Serial.print("[SD] Autosave Waktu Berhasil: ");
      Serial.println(now);
    }
  }
}

// ========================================================
// RENDER DISPLAY METODE REFRESH BARU (TEMA PUTIH)
// ========================================================
void updateDisplay() {
  tft.setRotation(0);
  tft.fillScreen(COLOR_BG);

  // ----------------------------------------------------
  // KONDISI A: STANDBY MINIMALIS MAKASSAR (TIDAK TERHUBUNG)
  // ----------------------------------------------------
  if (!deviceConnected) {
    struct tm *tmp = localtime(&lastUnixTime);
    if (lastUnixTime > 0) {
      // 1. Tampilkan Jam Besar Tanpa Header Atas ganda
      char bigTimeStr[6];
      sprintf(bigTimeStr, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
      tft.setTextColor(COLOR_ACCENT, COLOR_BG);
      tft.drawCentreString(bigTimeStr, tft.width() / 2, 95, 4);

      // 2. Tampilkan Tanggal Indonesia Lengkap dengan Hari
      char bigDateStr[50];
      sprintf(bigDateStr, "%s, %02d %s %d", hariIndo[tmp->tm_wday], tmp->tm_mday, bulanIndo[tmp->tm_mon], tmp->tm_year + 1900);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      tft.drawCentreString(bigDateStr, tft.width() / 2, 155, 2);
    }

    // 3. Status di Footer menggantikan identitas program
    tft.drawFastHLine(20, 290, tft.width() - 40, COLOR_BAR_BG);
    tft.setTextColor(COLOR_DATE, COLOR_BG);
    tft.drawCentreString("MENUNGGU KONEKSI HP...", tft.width() / 2, 302, 1);
    return;
  }

  // ----------------------------------------------------
  // KONDISI B: ACTIVE MODE DASHBOARD (TERHUBUNG)
  // ----------------------------------------------------
  // 1. Header Jam Kecil Atas
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawCentreString(currentTime, tft.width() / 2, 15, 4);
  tft.drawFastHLine(20, 55, tft.width() - 40, COLOR_ACCENT);

  // ========================================================
  // 2. PANEL ATAS: NAVIGASI ATAU OVERLAY NOTIFIKASI
  // ========================================================
  if (isNotify) {
    // Jika ada Notifikasi, ambil alih Space Navigasi (Menggunakan tema warna Notifikasi khusus)
    tft.fillRoundRect(8, 65, tft.width() - 16, 80, 8, tft.color565(240, 240, 240));  // Background Abu-abu Ringan
    tft.drawRoundRect(8, 65, tft.width() - 16, 80, 8, COLOR_STATUS);                 // Border Magenta

    tft.setTextColor(COLOR_STATUS, tft.color565(240, 240, 240));
    tft.drawString(notifySrc.substring(0, 15), 18, 72, 1);  // Sumber (App/WhatsApp)

    tft.setTextColor(COLOR_TEXT, tft.color565(240, 240, 240));
    String shortTitle = notifyTitle.length() > 16 ? notifyTitle.substring(0, 14) + ".." : notifyTitle;
    tft.drawString(shortTitle, 18, 88, 2);  // Judul/Pengirim

    tft.setTextColor(COLOR_ARTIST, tft.color565(240, 240, 240));
    String shortBody = notifyBody.length() > 24 ? notifyBody.substring(0, 22) + ".." : notifyBody;
    tft.drawString(shortBody, 18, 115, 1);  // Isi Pesan
  } else if (isNavigating) {
    // Jika tidak ada notifikasi dan GPS aktif, tampilkan Navigasi
    tft.fillRoundRect(8, 65, tft.width() - 16, 80, 8, COLOR_NAV_BG);
    tft.setTextColor(COLOR_NAV_TXT, COLOR_NAV_BG);
    tft.drawString("NAVIGASI", 18, 72, 1);
    tft.drawString(navDist, 18, 85, 2);

    String shortInstr = navInstr.length() > 18 ? navInstr.substring(0, 16) + ".." : navInstr;
    tft.drawCentreString(shortInstr, tft.width() / 2, 115, 2);
    if (navEta != "") tft.drawRightString("ETA: " + navEta, tft.width() - 18, 72, 1);
  } else {
    // Standby Navigasi kosong
    tft.drawRoundRect(8, 65, tft.width() - 16, 35, 8, COLOR_BAR_BG);
    tft.setTextColor(COLOR_ARTIST, COLOR_BG);
    tft.drawCentreString("Navigasi tidak aktif", tft.width() / 2, 75, 1);
  }

 // ========================================================
  // 3. PANEL BAWAH: MUSIC INFO ATAU OVERLAY CALL (TELEPON)
  // ========================================================
  int musicY = (isNavigating || isNotify) ? 155 : 115;

  if (isIncomingCall) {
    // ----------------------------------------------------
    // KONDISI CALL: Latar Merah Pastel Lembut, Teks Hitam
    // ----------------------------------------------------
    uint16_t lightRedBg   = tft.color565(255, 235, 235); // Merah pastel sangat lembut
    uint16_t softRedText  = tft.color565(180, 0, 0);     // Merah tua untuk sub-header
    uint16_t darkRedBorder= tft.color565(255, 100, 100);   // Garis tepi merah muda

    // Gambar Kotak Padat Berwarna Lembut + Border
    tft.fillRoundRect(5, musicY, tft.width() - 10, 85, 4, lightRedBg); 
    tft.drawRoundRect(5, musicY, tft.width() - 10, 85, 4, darkRedBorder); 
    
    // Teks Sub-Header (Merah Tua agar kontras)
    tft.setTextColor(softRedText, lightRedBg);
    tft.drawCentreString("PANGGILAN MASUK", tft.width() / 2, musicY + 12, 1);
    
    // Nama Penelepon (Hitam Pekat)
    tft.setTextColor(TFT_BLACK, lightRedBg);
    String shortCall = callName.length() > 16 ? callName.substring(0, 14) + ".." : callName;
    tft.drawCentreString(shortCall, tft.width() / 2, musicY + 32, 2); 
    
    // Petunjuk (Abu-abu Tua)
    tft.setTextColor(tft.color565(80, 80, 80), lightRedBg);
    tft.drawCentreString("Periksa HP Anda / Angkat", tft.width() / 2, musicY + 60, 1);
  } 
  else {
    // ----------------------------------------------------
    // KONDISI MUSIK: Latar Sesuai Event (Hijau/Oranye Pastel), Teks Hitam
    // ----------------------------------------------------
    uint16_t boxBgColor;
    uint16_t borderColor;
    uint16_t statusTextColor;

    if (musicState == "play") {
      boxBgColor      = tft.color565(230, 255, 235); // Hijau pastel sangat tipis
      borderColor     = tft.color565(100, 220, 140); // Border hijau lembut
      statusTextColor = tft.color565(0, 150, 70);    // Teks status hijau tua
    } else {
      boxBgColor      = tft.color565(255, 243, 225); // Oranye/Kuning pastel sangat tipis
      borderColor     = tft.color565(255, 180, 100); // Border oranye lembut
      statusTextColor = tft.color565(200, 100, 0);   // Teks status oranye tua
    }

    // Gambar Kotak Padat Berwarna Lembut + Border
    tft.fillRoundRect(5, musicY, tft.width() - 10, 85, 4, boxBgColor);
    tft.drawRoundRect(5, musicY, tft.width() - 10, 85, 4, borderColor);

    // Judul Musik Maksimal 24 Karakter (Teks Hitam Pekat)
    tft.setTextColor(TFT_BLACK, boxBgColor);
    String shortTitle = title.length() > 24 ? title.substring(0, 24) + ".." : title;
    tft.drawCentreString(shortTitle, tft.width() / 2, musicY + 15, 2);

    // Nama Artis (Teks Abu-abu Tua agar hierarkinya rapi)
    tft.setTextColor(tft.color565(70, 70, 70), boxBgColor);
    String shortArtist = artist.length() > 22 ? artist.substring(0, 20) + ".." : artist;
    tft.drawCentreString(shortArtist, tft.width() / 2, musicY + 42, 1);

    // Status Durasi Track (Warna disesuaikan tema event)
    tft.setTextColor(statusTextColor, boxBgColor);
    String statusTxt = (musicState == "play") ? "NOW PLAYING" : "PAUSED (" + formatTime(positionSec) + "/" + formatTime(durationSec) + ")";
    tft.drawCentreString(statusTxt, tft.width() / 2, musicY + 62, 1);
  }

  // 4. Panel Volume Bar
  int volY = 250;
  tft.setTextColor(COLOR_VOL, COLOR_BG);
  tft.drawCentreString("VOLUME " + String(volumeLevel) + "%", tft.width() / 2, volY, 1);
  tft.fillRoundRect((tft.width() - 160) / 2, volY + 15, 160, 6, 3, COLOR_BAR_BG);
  int progress = map(volumeLevel, 0, 100, 0, 160);
  if (progress > 0) tft.fillRoundRect((tft.width() - 160) / 2, volY + 15, progress, 6, 3, COLOR_BAR_FILL);

  // 5. TANGGAL INDONESIA (Paling Bawah)
  if (lastUnixTime > 0) {
    struct tm *tmp = localtime(&lastUnixTime);
    char dateBuf[50];
    sprintf(dateBuf, "%s, %02d %s %d", hariIndo[tmp->tm_wday], tmp->tm_mday, bulanIndo[tmp->tm_mon], tmp->tm_year + 1900);
    tft.setTextColor(COLOR_DATE, COLOR_BG);
    tft.drawCentreString(dateBuf, tft.width() / 2, 298, 1);
  }
}

// ========================================================
// CORE ADOPSI PARSING BUFFER METODE BARU
// ========================================================
void processBuffer(String data) {
  Serial.println("\n[PARSING DATA]");
  Serial.println(data);
  Serial.println("------------------------------------------------");

  // Handle Sinkronisasi Waktu Javascript + Offset WITA (+8 Jam)
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    lastUnixTime = data.substring(startIdx, endIdx).toInt();

    if (lastUnixTime > 0) {
      lastUnixTime += 28800;  // Injeksi +8 Jam manual khusus Area Makassar

      struct timeval tv;
      tv.tv_sec = lastUnixTime;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);

      struct tm *tmp = localtime(&lastUnixTime);
      char buf[10];
      sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
      currentTime = String(buf);

      saveTimeToSD();  // Simpan timestamp segar ke MicroSD
      refreshDisplay = true;
    }
    return;
  }

  // Handle Data Objek JSON Gadgetbridge
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
        musicState = doc["state"].as<String>();
        positionSec = doc["position"] | 0;
        refreshDisplay = true;
      } else if (type == "nav") {
        navInstr = doc["instr"] | "";
        String rawDist = doc["distance"] | "";
        if (rawDist.length() >= 2) rawDist.setCharAt(rawDist.length() - 2, ' ');
        navDist = rawDist;
        navEta = doc["eta"] | "";
        isNavigating = (navInstr != "" && navInstr != " " && navInstr != "null");
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
          refreshDisplay = true;  // <-- TAMBAHKAN INI
          lastNotifyMillis = millis(); // <-- Catat waktu pesan masuk
        }
      } else if (type == "call") {
        String cmd = doc["cmd"] | "";
        if (cmd == "incoming") {
          callName = doc["name"] | "Unknown";
          isIncomingCall = true;
        } else if (cmd == "end" || cmd == "accept") {
          isIncomingCall = false;
        }
        refreshDisplay = true;  // <-- TAMBAHKAN INI
      }
    }
  }
}

void sendToGB(String cmd) {
  if (deviceConnected) {
    pCharacteristicTX->setValue(cmd.c_str());
    pCharacteristicTX->notify();
    Serial.print("Sent to GB: " + cmd);
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

    // TRIGGER JABAT TANGAN AKTIF (Mengadopsi pola coding baru)
    delay(500);
    sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"ESP32-Dashboard\"})\n");
    sendToGB("GB({\"t\":\"gps\",\"status\":\"ok\"})\n");
    sendToGB("GB({\"t\":\"music\",\"n\":\"info\"})\n");
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    refreshDisplay = true;
    pServer->getAdvertising()->start();
  }
};

void setup() {
  Serial.begin(115200);

  // Pemicu Backlight Layar CYD
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(100);

  tft.init();
  tft.setRotation(0);

  // Inisialisasi Slot MicroSD CYD
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] MicroSD tidak terdeteksi.");
  } else {
    Serial.println("[SD] MicroSD Terbaca.");
    loadTimeFromSD();  // Sinkronkan RTC internal dari histori file SD card
  }

  // Setup BLE Terpusat
  BLEDevice::init("Bangle.js");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  updateDisplay();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Sinkronisasi transisi koneksi dinamis
  if (deviceConnected != lastConnectionState) {
    lastConnectionState = deviceConnected;
    refreshDisplay = true;
  }

  // 2. Loop penambah menit RTC Mandiri - DIUBAH MENJADI SETIAP 50 DETIK
  if (currentMillis - lastClockUpdate > 50000) {
    time_t now;
    time(&now);
    lastUnixTime = now;

    struct tm *tmp = localtime(&lastUnixTime);
    char buf[10];
    sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
    currentTime = String(buf);

    refreshDisplay = true;
    lastClockUpdate = currentMillis;
  }

  // 3. BACKUP: Tulis waktu ke file config MicroSD sekali per menit
  if (currentMillis - lastSDWrite > 60000) {
    saveTimeToSD();
    lastSDWrite = currentMillis;
  }

  // 4. Eksekusi Gambar Ulang Layar (Juga otomatis memperbarui Jam saat refreshDisplay aktif)
  if (refreshDisplay) {
    // Ambil waktu paling aktual tepat saat updateDisplay dipicu data eksternal
    time_t now;
    time(&now);
    struct tm *tmp = localtime(&now);
    char buf[10];
    sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
    currentTime = String(buf);

    updateDisplay();
    refreshDisplay = false;
  }

  // Auto-clear notifikasi setelah 8 detik (8000 ms) agar map navigasi muncul kembali
  if (isNotify && (currentMillis - lastNotifyMillis > 8000)) {
    isNotify = false;
    refreshDisplay = true;
  }
  
  delay(10);
}