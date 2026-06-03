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

BLEServer* pServer = NULL;
bool deviceConnected = false;
String bleBuffer = "";

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define SD_CS 5  // Pin CS MicroSD bawaan CYD

// Variabel Konten Dashboard
String currentTrack = "Tidak Ada Lagu";
String currentArtist = "-";
String musicState = "STOP";

unsigned long lastClockUpdate = 0;
unsigned long lastSDWrite = 0;

// Daftar nama bulan Indonesia
const char* bulanIndo[] = { "Januari", "Februari", "Maret", "April", "Mei", "Juni",
                            "Juli", "Agustus", "September", "Oktober", "November", "Desember" };

bool lastConnectionState = false;

void drawCenteredString(String text, int y, int fontSize) {
  tft.setTextSize(fontSize);
  int textWidth = tft.textWidth(text);
  int x = (240 - textWidth) / 2;
  tft.drawString(text, x, y);
}

// ========================================================
// FUNGSI MANAGEMENT FILE CONFIG (SD CARD)
// ========================================================
void loadTimeFromSD() {
  File file = SD.open("/config.txt", FILE_READ);
  if (!file) {
    Serial.println("[SD] File config.txt tidak ditemukan. Menggunakan waktu default.");
    return;
  }

  String savedTimeStr = file.readStringUntil('\n');
  file.close();

  time_t savedTimestamp = (time_t)savedTimeStr.toInt();
  if (savedTimestamp > 1000000000) {  // Validasi timestamp masuk akal
    struct timeval tv;
    tv.tv_sec = savedTimestamp;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    Serial.print("[SD] Sukses memuat waktu terakhir aktif: ");
    Serial.println(savedTimestamp);
  }
}

void saveTimeToSD() {
  time_t now;
  time(&now);

  // Hanya simpan jika jam internal sudah ter-update (di atas tahun 2020)
  if (now > 1600000000) {
    File file = SD.open("/config.txt", FILE_WRITE);
    if (file) {
      file.println(now);
      file.close();
      Serial.print("[SD] Autosave Waktu Berhasil: ");
      Serial.println(now);
    } else {
      Serial.println("[SD] Gagal menulis ke config.txt");
    }
  }
}

void updateClockDisplay() {
  // Hanya gambar jam kecil di header ATAS jika HP TERHUBUNG
  if (!deviceConnected) return; 

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawCenteredString("--:--", 15, 3);
    return;
  }

  char timeString[6];
  sprintf(timeString, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  drawCenteredString(timeString, 15, 3);
}

// Fungsi Update Konten Utama (Dinamis Berdasarkan Koneksi)
void updateDisplay() {
  // 1. Bersihkan seluruh area layar dari bawah garis ungu (y=5) hingga paling bawah (y=320)
  tft.fillRect(0, 5, 240, 315, TFT_BLACK); 

  // ========================================================
  // KONDISI A: TIDAK ADA PERANGKAT TERHUBUNG (STANDBY MODE)
  // ========================================================
  if (!deviceConnected) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    // Jam Besar di Tengah Layar (Geser sedikit ke atas agar lebih proporsional)
    char bigTimeStr[6];
    sprintf(bigTimeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    tft.setTextColor(tft.color565(0, 220, 255), TFT_BLACK); // Neon Cyan
    drawCenteredString(bigTimeStr, 100, 4); 

    // Tanggal Indonesia Besar di Bawah Jam
    char bigDateStr[30];
    sprintf(bigDateStr, "%d %s %d", timeinfo.tm_mday, bulanIndo[timeinfo.tm_mon], timeinfo.tm_year + 1900);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawCenteredString(bigDateStr, 155, 2); 

    // FOOTER STANDBY: Garis pembatas bawah & Status menggantikan MOTO DASH v1.0
    tft.drawFastHLine(0, 295, 240, tft.color565(60, 60, 60));
    tft.setTextColor(tft.color565(120, 120, 120), TFT_BLACK); // Muted Grey
    drawCenteredString("MENUNGGU KONEKSI HP...", 305, 1);
    return; 
  }

  // ========================================================
  // KONDISI B: PERANGKAT TERHUBUNG (ACTIVE DASHBOARD MODE)
  // ========================================================
  // Gambar ulang garis pemisah header karena sempat tersapu fillRect saat transisi
  tft.drawFastHLine(0, 45, 240, tft.color565(80, 80, 80)); 

  // SECTION 1: STATUS MUSIK
  tft.setTextColor(tft.color565(0, 255, 150), TFT_BLACK);
  if (musicState == "play") {
    drawCenteredString("• PLAYING NOW •", 55, 1);
  } else {
    tft.setTextColor(tft.color565(255, 50, 50), TFT_BLACK);
    drawCenteredString("• PAUSED •", 55, 1);
  }

  // SECTION 2: INFO LAGU (CARD STYLE)
  tft.drawRoundRect(15, 75, 210, 105, 8, tft.color565(60, 60, 60)); 
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  drawCenteredString(currentTrack.substring(0, 16), 100, 2);
  tft.setTextColor(tft.color565(0, 220, 255), TFT_BLACK);
  drawCenteredString(currentArtist.substring(0, 20), 135, 1);

  // SECTION 3: TANGGAL & NAVIGASI
  char dateString[30];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    sprintf(dateString, "%d %s %d", timeinfo.tm_mday, bulanIndo[timeinfo.tm_mon], timeinfo.tm_year + 1900);
    tft.setTextColor(tft.color565(200, 200, 200), TFT_BLACK);
    drawCenteredString(dateString, 195, 1);
  }

  tft.drawFastHLine(20, 220, 200, tft.color565(40, 40, 40));
  tft.setTextColor(tft.color565(180, 180, 180), TFT_BLACK);
  drawCenteredString("NAVIGASI", 230, 1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  drawCenteredString("Ikuti Jalur Utama", 255, 2); 

  // FOOTER ACTIVE: Kembalikan tulisan MOTO DASH V1.0 saat terhubung
  tft.drawFastHLine(0, 295, 240, tft.color565(80, 80, 80));
  tft.setTextColor(tft.color565(100, 100, 100), TFT_BLACK);
  drawCenteredString("MOTO DASH V1.0", 305, 1);
}

// ========================================================
// PARSER DATA GADGETBRIDGE WITH MANUAL WITA OFFSET (+8 JAM)
// ========================================================
void parseGadgetbridgeData(String data) {
  Serial.println("\n[PARSING DATA]");
  Serial.println(data);
  Serial.println("------------------------------------------------");

  // Handle Sinkronisasi via JS mentah setTime()
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("setTime(") + 8;
    int endIdx = data.length();
    for (int i = startIdx; i < data.length(); i++) {
      if (data.charAt(i) == ')') {
        endIdx = i;
        break;
      }
    }

    String timestampStr = data.substring(startIdx, endIdx);
    time_t timestamp = (time_t)timestampStr.toInt();

    if (timestamp > 0) {
      // TAMBAHKAN OFFSET MANUAL +8 JAM UNTUK MAKASSAR (WITA)
      // 8 jam = 8 * 3600 detik = 28800 detik
      timestamp += 28800;

      struct timeval tv;
      tv.tv_sec = timestamp;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL);

      Serial.println("-> Waktu WITA Berhasil Disinkronkan!");
      updateClockDisplay();
      saveTimeToSD();  // Langsung amankan ke SD begitu mendapat update akurat
    }
    return;
  }

  // Handle Data Objek JSON (Musik)
  int startIdx = data.indexOf('{');
  int endIdx = data.lastIndexOf('}');
  if (startIdx == -1 || endIdx == -1) return;
  String jsonStr = data.substring(startIdx, endIdx + 1);

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) return;

  String type = doc["t"].as<String>();

  if (type == "musicinfo") {
    currentTrack = doc["track"].as<String>();
    currentArtist = doc["artist"].as<String>();
    updateDisplay();
  } else if (type == "musicstate") {
    musicState = doc["state"].as<String>();
    updateDisplay();
  }
}

// ... (Callback BLE MyServerCallbacks & MyCallbacks tetap sama persis) ...
// Callback BLE Server untuk Mendeteksi Transisi Hubung/Putus secara Real-Time
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      bleBuffer = "";
      Serial.println("\n[STATUS] Gadgetbridge Terhubung!");
      // Kita tidak panggil updateDisplay() langsung di sini karena data waktu 
      // dari HP biasanya langsung masuk beberapa milidetik setelah terhubung.
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = true; // Siasat sesaat agar loop tahu ada transisi
      deviceConnected = false; 
      Serial.println("\n[STATUS] Gadgetbridge Terputus.");
      updateDisplay(); // Langsung ganti ke mode jam besar standby
      pServer->getAdvertising()->start();
    }
};
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String chunk = pCharacteristic->getValue().c_str();
    bleBuffer += chunk;
    if (bleBuffer.indexOf('\n') != -1) {
      bleBuffer.trim();
      if (bleBuffer.length() > 0) parseGadgetbridgeData(bleBuffer);
      bleBuffer = "";
    }
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  delay(100);

  tft.init();
  tft.setRotation(0); // Mode Vertikal
  tft.fillScreen(TFT_BLACK);
  
  // Aksen Ungu Amethyst paling atas (tetap ada di kedua mode)
  tft.fillRect(0, 0, 240, 4, tft.color565(150, 0, 255));

  // Inisialisasi MicroSD
  if(!SD.begin(SD_CS)){
    Serial.println("[SD] Pasang / Gagal membaca MicroSD!");
  } else {
    Serial.println("[SD] MicroSD Terbaca dengan Baik.");
    loadTimeFromSD();
  }

  // Panggil update pertama kali (akan otomatis masuk mode Standby Jam Besar)
  updateDisplay();

  // Setup BLE
  BLEDevice::init("Bangle.js CYD");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pTxCharacteristic = pService->createCharacteristic("6e400003-b5a3-f393-e0a9-e50e24dcca9e", BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Deteksi jika ada perubahan status koneksi untuk memicu redraw layar penuh
  if (deviceConnected != lastConnectionState) {
    lastConnectionState = deviceConnected;
    updateDisplay();
  }

  // 2. Update Jam dan Tanggal di layar secara berkala
  if (currentMillis - lastClockUpdate > 5000) {
    if (deviceConnected) {
      updateClockDisplay(); // Hanya update jam kecil di header jika terhubung
    } else {
      updateDisplay();      // Update jam besar tengah jika sedang standby
    }
    lastClockUpdate = currentMillis;
  }

  // 3. AUTOSAVE: Perbarui jalannya waktu ke MicroSD setiap menit
  if (currentMillis - lastSDWrite > 60000) {
    saveTimeToSD();
    lastSDWrite = currentMillis;
  }

  delay(10);
}