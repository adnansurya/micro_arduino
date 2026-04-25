#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristicTX;
String artist = "No Artist";
String title = "No Title";
String currentTime = "00:00";
String inputBuffer = "";

bool refreshDisplay = false;  // Tanda untuk update layar

void updateDisplay() {
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(4);
  tft.drawCentreString(currentTime, tft.width() / 2, 30, 1);

  tft.drawFastHLine(20, 80, tft.width() - 40, TFT_DARKGREY);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  String displayTitle = title.length() > 18 ? title.substring(0, 15) + "..." : title;
  tft.drawCentreString(displayTitle, tft.width() / 2, 100, 1);

  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawCentreString(artist, tft.width() / 2, 130, 1);

  tft.setTextColor(TFT_WHITE);
  // Tombol Media (Sudah Benar 6 Argumen)
  tft.fillRoundRect(10, 170, 60, 45, 5, TFT_BLUE);
  tft.drawCentreString("<<", 40, 185, 2);

  tft.fillRoundRect(80, 170, 80, 45, 5, TFT_MAROON);
  tft.drawCentreString("P/||", 120, 185, 2);

  tft.fillRoundRect(170, 170, 60, 45, 5, TFT_BLUE);
  tft.drawCentreString(">>", 200, 185, 2);

  // --- PERBAIKAN DI SINI: Menambahkan angka 5 sebagai radius ---
  tft.fillRoundRect(10, 230, 105, 40, 5, TFT_DARKCYAN);
  tft.drawCentreString("VOL -", 62, 243, 1);

  tft.fillRoundRect(125, 230, 105, 40, 5, TFT_DARKCYAN);
  tft.drawCentreString("VOL +", 177, 243, 1);
}

void sendCommand(String cmd) {
  String fullCmd = "GB(" + cmd + ")\n";
  pCharacteristicTX->setValue(fullCmd.c_str());
  pCharacteristicTX->notify();
}

void syncTime(long timestamp) {
  time_t t = timestamp;
  struct tm *tmp = localtime(&t);
  char buf[10];
  sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
  currentTime = String(buf);
  refreshDisplay = true;  // Jangan panggil updateDisplay() langsung di sini
}

void processBuffer(String data) {
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    String tsStr = data.substring(startIdx, endIdx);
    syncTime(tsStr.toInt());
    return;
  }

  int start = data.indexOf("GB({");
  int end = data.lastIndexOf("})");
  if (start != -1 && end != -1) {
    String jsonStr = data.substring(start + 3, end + 2);
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (!error) {
      String type = doc["t"].as<String>();
      if (type == "musicinfo") {
        title = doc["track"] | "Unknown Title";
        artist = doc["artist"] | "Unknown Artist";
        refreshDisplay = true;  // Tandai untuk update
      }
    }
  }
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // Gunakan std::string untuk kompatibilitas library BLE terbaru
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
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  uint16_t calData[5] = { 444, 2897, 423, 3309, 4 };
  tft.setTouch(calData);
  updateDisplay();

  BLEDevice::init("Bangle.js");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pService->start();
  pServer->getAdvertising()->start();
}

void loop() {
  // 1. Tangani Update Layar (Diluar Callback BLE)
  if (refreshDisplay) {
    updateDisplay();
    refreshDisplay = false;
  }

  // 2. Tangani Touch dengan jeda agar tidak membebani CPU
  static uint32_t lastTouch = 0;
  if (millis() - lastTouch > 50) {
    uint16_t t_x = 0, t_y = 0;
    if (tft.getTouch(&t_x, &t_y)) {
      // Logika tombol Anda di sini...

      // Setelah sendCommand, beri delay agar tidak bentrok dengan BLE transmission
      lastTouch = millis() + 200;
    }
    lastTouch = millis();
  }

  // Beri kesempatan sistem untuk memproses Bluetooth background task
  delay(1);
}