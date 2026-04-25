#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristicTX;
bool deviceConnected = false;
String artist = "-", title = "-", currentTime = "00:00";
String navInstr = "", navDist = "", navEta = "";
int volumeLevel = 0;
String inputBuffer = "";
bool refreshDisplay = true;
bool isNavigating = false;

// --- TAMPILAN HALAMAN ---

void drawStandbyPage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(2);
  tft.drawCentreString("DISCONNECTED", tft.width() / 2, 100, 1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLUE);
  tft.drawCentreString("Waiting for Gadgetbridge...", tft.width() / 2, 140, 1);
}

void updateDisplay() {
  if (!deviceConnected) {
    drawStandbyPage();
    return;
  }

  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  // 1. Header Jam
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(4);
  tft.drawCentreString(currentTime, tft.width() / 2, 20, 1);
  tft.drawFastHLine(20, 65, tft.width() - 40, TFT_BLUE);

  // 2. Panel Navigasi
  if (isNavigating) {
    tft.fillRoundRect(5, 75, tft.width()-10, 100, 8, TFT_DARKCYAN);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString("NAVIGASI", 15, 85);
    
    // Jarak
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(navDist, 15, 100);
    
    // Instruksi (Size 2)
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    String shortInstr = navInstr.length() > 18 ? navInstr.substring(0, 16) + ".." : navInstr;
    tft.drawCentreString(shortInstr, tft.width() / 2, 130, 1);

    // ETA
    if (navEta != "") {
      tft.setTextSize(1);
      tft.setTextColor(TFT_SILVER);
      tft.drawRightString("ETA: " + navEta, tft.width() - 15, 85, 1);
    }
  }

  // 3. Panel Musik
  int musicY = isNavigating ? 195 : 120;
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  String shortTitle = title.length() > 16 ? title.substring(0, 14) + ".." : title;
  tft.drawCentreString(shortTitle, tft.width() / 2, musicY, 1);
  
  tft.setTextColor(TFT_SILVER);
  tft.setTextSize(1);
  tft.drawCentreString(artist, tft.width() / 2, musicY + 30, 1);

  // 4. Volume
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.drawCentreString("VOLUME", tft.width() / 2, 275, 1);
  tft.setTextSize(3);
  tft.drawCentreString(String(volumeLevel) + "%", tft.width() / 2, 290, 1);
}

// --- CALLBACK SERVER ---

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; refreshDisplay = true; }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      refreshDisplay = true;
      pServer->getAdvertising()->start();
    }
};

void processBuffer(String data) {
  // Selalu print data yang diterima ke Serial Monitor
  Serial.print("Data Received: ");
  Serial.println(data);

  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    time_t t = data.substring(startIdx, endIdx).toInt();
    struct tm *tmp = localtime(&t);
    char buf[10];
    sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
    currentTime = String(buf);
    refreshDisplay = true;
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
        refreshDisplay = true;
      } 
      else if (type == "nav") {
        navInstr = doc["instr"] | "";
        String rawDist = doc["distance"] | "";
        
        // Logika ganti karakter kedua dari ujung menjadi spasi
        if (rawDist.length() >= 2) {
            rawDist.setCharAt(rawDist.length() - 2, ' ');
        }
        navDist = rawDist;
        
        navEta = doc["eta"] | "";
        isNavigating = (navInstr != "");
        refreshDisplay = true;
      }
      else if (type == "audio") {
        volumeLevel = doc["v"];
        refreshDisplay = true;
      }
    }
  }
}

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    for (char c : value) {
      if (c == '\n') { processBuffer(inputBuffer); inputBuffer = ""; }
      else { inputBuffer += c; }
    }
  }
};

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  
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
  if (refreshDisplay) {
    updateDisplay();
    refreshDisplay = false;
  }
  delay(50);
}