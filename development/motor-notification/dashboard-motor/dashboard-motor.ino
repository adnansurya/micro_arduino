#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <time.h> // Library internal ESP32 untuk waktu

TFT_eSPI tft = TFT_eSPI();

BLEServer* pServer = NULL;
bool deviceConnected = false;
String bleBuffer = ""; 

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID                "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

// Variabel Konten Dashboard
String currentTrack = "Tidak Ada Lagu";
String currentArtist = "-";
String musicState = "STOP";
unsigned long lastClockUpdate = 0;

void drawCenteredString(String text, int y, int fontSize) {
  tft.setTextSize(fontSize);
  int textWidth = tft.textWidth(text);
  int x = (240 - textWidth) / 2;
  tft.drawString(text, x, y);
}

// Fungsi Khusus untuk memperbarui Jam di Header secara Real-Time
void updateClockDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // Jika RTC internal belum disinkronkan oleh HP
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawCenteredString("--:--", 15, 3);
    return;
  }

  // Format waktu menjadi Jam:Menit (contoh: 13:45)
  char timeString[6];
  sprintf(timeString, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  drawCenteredString(timeString, 15, 3);
}

// Fungsi Update Konten Utama
void updateDisplay() {
  tft.fillRect(0, 45, 240, 245, TFT_BLACK); 

  // SECTION 1: STATUS KONEKSI & MUSIK
  if (deviceConnected) {
    tft.setTextColor(tft.color565(0, 255, 150), TFT_BLACK);
    if (musicState == "play") {
      drawCenteredString("• PLAYING NOW •", 55, 1);
    } else {
      tft.setTextColor(tft.color565(255, 50, 50), TFT_BLACK);
      drawCenteredString("• PAUSED •", 55, 1);
    }
  }

  // SECTION 2: INFO LAGU
  tft.drawRoundRect(15, 75, 210, 105, 8, tft.color565(60, 60, 60)); 
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String trackShort = currentTrack.substring(0, 16);
  drawCenteredString(trackShort, 100, 2);

  tft.setTextColor(tft.color565(0, 220, 255), TFT_BLACK);
  String artistShort = currentArtist.substring(0, 20);
  drawCenteredString(artistShort, 135, 1);

  // SECTION 3: NAVIGASI
  tft.drawFastHLine(20, 200, 200, tft.color565(40, 40, 40));
  tft.setTextColor(tft.color565(180, 180, 180), TFT_BLACK);
  drawCenteredString("NAVIGASI", 215, 1);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  drawCenteredString("Ikuti Jalur Utama", 240, 2); 
}

// Fungsi Parser Data Gadgetbridge
void parseGadgetbridgeData(String data) {
  // SELALU TAMPILKAN TEKS YANG AKAN DI-PARSE KE SERIAL MONITOR
  Serial.println("\n[PARSING DATA]");
  Serial.println(data);
  Serial.println("------------------------------------------------");

  // ========================================================
  // KONDISI 1: PARSING FORMAT JAVASCRIPT MENTAH "setTime(XXXXX)"
  // ========================================================
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("setTime(") + 8; // Geser setelah teks "setTime("
    int endIdx = data.length();
    
    // Cari posisi tanda kurung penutup ')' setelah angka timestamp
    for (int i = startIdx; i < data.length(); i++) {
      if (data.charAt(i) == ')') {
        endIdx = i;
        break;
      }
    }

    String timestampStr = data.substring(startIdx, endIdx);
    time_t timestamp = (time_t)timestampStr.toInt(); // Konversi string angka ke integer

    if (timestamp > 0) {
      struct timeval tv;
      tv.tv_sec = timestamp;
      tv.tv_usec = 0;
      settimeofday(&tv, NULL); // Sinkronkan RTC Internal ESP32
      
      Serial.println("-> Waktu Berhasil Disinkronkan via setTime()!");
      updateClockDisplay();
    }
    return; // Keluar dari fungsi karena data sudah berhasil diproses
  }

  // ========================================================
  // KONDISI 2: PARSING FORMAT JSON "GB({...})" (Untuk Musik/Navigasi)
  // ========================================================
  int startIdx = data.indexOf('{');
  int endIdx = data.lastIndexOf('}');
  if (startIdx == -1 || endIdx == -1) return;
  
  String jsonStr = data.substring(startIdx, endIdx + 1);

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    Serial.print("Gagal parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }

  String type = doc["t"].as<String>();

  // Backup jika suatu saat format waktu dikirim dalam bentuk JSON objek
  if (type == "time") {
    time_t timestamp = doc["time"].as<time_t>();
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    Serial.println("-> Waktu Berhasil Disinkronkan via JSON!");
    updateClockDisplay();
  }
  // Handle Info Lagu
  else if (type == "musicinfo") {
    currentTrack = doc["track"].as<String>();
    currentArtist = doc["artist"].as<String>();
    updateDisplay();
  }
  // Handle Status Musik
  else if (type == "musicstate") {
    musicState = doc["state"].as<String>();
    updateDisplay();
  }
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      bleBuffer = "";
      Serial.println("\n[STATUS] Gadgetbridge Terhubung!");
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("\n[STATUS] Gadgetbridge Terputus.");
      pServer->getAdvertising()->start();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String chunk = pCharacteristic->getValue().c_str();
      bleBuffer += chunk;

      if (bleBuffer.indexOf('\n') != -1) {
        bleBuffer.trim(); 
        if (bleBuffer.length() > 0) {
          parseGadgetbridgeData(bleBuffer);
        }
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
  tft.setRotation(0); // Mode Vertikal (Portrait)
  tft.fillScreen(TFT_BLACK);
  
  // Header Statis
  tft.fillRect(0, 0, 240, 4, tft.color565(150, 0, 255));
  tft.drawFastHLine(0, 45, 240, tft.color565(80, 80, 80));
  updateClockDisplay(); // Tampilkan "--:--" dulu sebelum sinkron

  // Footer Statis
  tft.drawFastHLine(0, 295, 240, tft.color565(80, 80, 80));
  tft.setTextColor(tft.color565(100, 100, 100), TFT_BLACK);
  drawCenteredString("MOTO DASH V1.0", 305, 1);

  updateDisplay();

  // Setup BLE
  BLEDevice::init("Bangle.js CYD");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  BLECharacteristic *pTxCharacteristic = pService->createCharacteristic(
                        "6e400003-b5a3-f393-e0a9-e50e24dcca9e",
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                        RX_UUID,
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_WRITE_NR
                      );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  
  Serial.println("Sistem Siap. Menunggu data dari Gadgetbridge...");
}

void loop() {
  // Update jam di layar setiap 5 detik agar efisien dan tidak mengedip (flicker)
  if (millis() - lastClockUpdate > 5000) {
    updateClockDisplay();
    lastClockUpdate = millis();
  }
  delay(10);
}