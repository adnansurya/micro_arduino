#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

// UUID Standar Nordic UART untuk Bangle.js
#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristicTX;
String artist = "No Artist";
String title = "No Title";
String currentTime = "00:00";
String inputBuffer = "";
bool refreshDisplay = true;

// --- FUNGSI TAMPILAN ---
void updateDisplay() {
  tft.setRotation(0); // Mode Portrait
  tft.fillScreen(TFT_BLACK);
  
  // Tampilan Jam (Besar di Atas)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(4);
  tft.drawCentreString(currentTime, tft.width() / 2, 40, 1);

  // Garis Pemisah dekoratif
  tft.drawFastHLine(20, 100, tft.width() - 40, TFT_BLUE);

  // Judul Lagu
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  // Potong judul jika terlalu panjang agar tidak berantakan
  String shortTitle = title.length() > 16 ? title.substring(0, 14) + ".." : title;
  tft.drawCentreString(shortTitle, tft.width() / 2, 130, 1);
  
  // Nama Artis
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawCentreString(artist, tft.width() / 2, 160, 1);

  // Status Koneksi Sederhana di Bawah
  tft.setTextColor(TFT_DARKGREY);
  tft.drawCentreString("Gadgetbridge Connected", tft.width() / 2, 280, 1);
}

// --- LOGIKA DATA ---
void syncTime(long timestamp) {
  time_t t = timestamp;
  struct tm *tmp = localtime(&t);
  char buf[10];
  sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
  currentTime = String(buf);
  refreshDisplay = true;
}

void processBuffer(String data) {
  // Parsing Waktu dari perintah setTime HP
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    String tsStr = data.substring(startIdx, endIdx);
    syncTime(tsStr.toInt());
    return;
  }

  // Parsing JSON untuk Informasi Musik
  int start = data.indexOf("GB({");
  int end = data.lastIndexOf("})");

  if (start != -1 && end != -1) {
    String jsonStr = data.substring(start + 3, end + 2);
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (!error) {
      String type = doc["t"] | "";
      if (type == "musicinfo") {
        title = doc["track"] | "Unknown Title";
        artist = doc["artist"] | "Unknown Artist";
        refreshDisplay = true;
      }
    }
  }
}

// --- BLE CALLBACKS ---
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      for (int i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c == '\n') {
          processBuffer(inputBuffer);
          inputBuffer = "";
        } else {
          inputBuffer += c;
        }
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  
  // Setup Layar
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  updateDisplay();

  // Setup BLE
  BLEDevice::init("Bangle.js");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("Mode Display Aktif. Hubungkan via Gadgetbridge.");
}

void loop() {
  // Hanya update layar jika ada data baru masuk (flag refreshDisplay aktif)
  if (refreshDisplay) {
    updateDisplay();
    refreshDisplay = false;
  }
  
  // Memberi waktu untuk proses background BLE
  delay(10); 
}