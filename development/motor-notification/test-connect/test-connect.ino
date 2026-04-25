#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>  // Tambahkan untuk konversi waktu

TFT_eSPI tft = TFT_eSPI();

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristicTX;
String artist = "No Artist";
String title = "No Title";
String currentTime = "00:00";
String inputBuffer = "";

void updateDisplay() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  // Jam Besar
  tft.setTextSize(4);
  tft.drawCentreString(currentTime, tft.width() / 2, 20, 1);

  // Info Media
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN);
  tft.drawCentreString("Music:", tft.width() / 2, 80, 1);

  tft.setTextColor(TFT_YELLOW);
  tft.drawCentreString(title.substring(0, 20), tft.width() / 2, 110, 1);
  tft.setTextSize(1);
  tft.drawCentreString(artist, tft.width() / 2, 140, 1);
}

// Fungsi konversi UNIX timestamp ke format jam:menit
void syncTime(long timestamp) {
  time_t t = timestamp;
  struct tm *tmp = localtime(&t);
  char buf[10];
  sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
  currentTime = String(buf);
  updateDisplay();
}

void processBuffer(String data) {
  Serial.print("Processing: ");
  Serial.println(data);

  // 1. Cek jika data adalah setTime(12345678)
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    String tsStr = data.substring(startIdx, endIdx);
    syncTime(tsStr.toInt());
    return;
  }

  // 2. Cek jika data adalah JSON GB({...})
  int start = data.indexOf("GB({");
  int end = data.lastIndexOf("})");

  if (start != -1 && end != -1) {
    String jsonStr = data.substring(start + 3, end + 2);
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (!error) {
      String type = doc["t"].as<String>();

      if (type == "musicinfo") {
        title = doc["track"].as<String>();
        artist = doc["artist"].as<String>();
        updateDisplay();
      } else if (type == "musicstate") {
        // Bisa digunakan untuk deteksi Play/Pause
      }
    }
  }
}

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue().c_str();
    if (rxValue.length() > 0) {
      for (int i = 0; i < rxValue.length(); i++) {
        char c = rxValue[i];
        if (c == '\n') {  // Bangle.js biasanya kirim \n sebagai akhir perintah
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
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  updateDisplay();

  BLEDevice::init("Bangle.js");  // Gunakan nama persis Bangle.js agar lebih stabil
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  // Kirim status awal agar HP tahu kita siap menerima data
  delay(2000);
  pCharacteristicTX->setValue("GB({\"t\":\"status\",\"bat\":100})\n");
  pCharacteristicTX->notify();
}

void loop() {}