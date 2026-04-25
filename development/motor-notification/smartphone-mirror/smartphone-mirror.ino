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
String musicState = "pause";
int volumeLevel = 0;
long positionSec = 0, durationSec = 0;
String inputBuffer = "";
bool refreshDisplay = true;
bool isNavigating = false;

// Fungsi bantu konversi detik ke mm:ss
String formatTime(long seconds) {
  int m = seconds / 60;
  int s = seconds % 60;
  char buf[10];
  sprintf(buf, "%02d:%02d", m, s);
  return String(buf);
}

void updateDisplay() {
  if (!deviceConnected) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextSize(2);
    tft.drawCentreString("DISCONNECTED", tft.width() / 2, 100, 1);
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
    tft.fillRoundRect(5, 75, tft.width()-10, 90, 8, TFT_DARKCYAN);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.drawString("NAVIGASI", 15, 82);
    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(2);
    tft.drawString(navDist, 15, 95);
    tft.setTextColor(TFT_WHITE);
    String shortInstr = navInstr.length() > 18 ? navInstr.substring(0, 16) + ".." : navInstr;
    tft.drawCentreString(shortInstr, tft.width() / 2, 125, 1);
    if (navEta != "") {
      tft.setTextSize(1);
      tft.drawRightString("ETA: " + navEta, tft.width() - 15, 82, 1);
    }
  }

  // 3. Panel Musik
  int musicY = isNavigating ? 180 : 100;
  
  // LOGIKA STATE: Play vs Pause
  if (musicState == "play") {
    tft.setTextColor(TFT_GOLD);
    tft.setTextSize(1);
    tft.drawCentreString("NOW PLAYING", tft.width() / 2, musicY, 1);
    
    // Animasi Kecil (3 Baris naik turun acak setiap refresh)
    for(int i=0; i<3; i++) {
      int h = random(5, 15);
      tft.fillRect(tft.width()/2 - 15 + (i*12), musicY + 15, 6, h, TFT_GOLD);
    }
  } else {
    tft.setTextColor(TFT_ORANGE);
    tft.setTextSize(1);
    String pauseText = "PAUSED (" + formatTime(positionSec) + "/" + formatTime(durationSec) + ")";
    tft.drawCentreString(pauseText, tft.width() / 2, musicY, 1);
  }

  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  String shortTitle = title.length() > 16 ? title.substring(0, 14) + ".." : title;
  tft.drawCentreString(shortTitle, tft.width() / 2, musicY + 30, 1);
  
  tft.setTextColor(TFT_SILVER);
  tft.setTextSize(1);
  tft.drawCentreString(artist, tft.width() / 2, musicY + 55, 1);

  // 4. Volume
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.drawCentreString("VOLUME " + String(volumeLevel) + "%", tft.width() / 2, 290, 1);
}

void processBuffer(String data) {
  Serial.print("Raw: "); Serial.println(data);

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
        durationSec = doc["dur"] | 0; // Simpan durasi total
        refreshDisplay = true;
      } 
      else if (type == "musicstate") {
        musicState = doc["state"].as<String>(); // "play" atau "pause"
        positionSec = doc["position"] | 0;      // Posisi detik sekarang
        refreshDisplay = true;
      }
      else if (type == "nav") {
        navInstr = doc["instr"] | "";
        String rawDist = doc["distance"] | "";
        if (rawDist.length() >= 2) rawDist.setCharAt(rawDist.length() - 2, ' ');
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

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; refreshDisplay = true; }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      refreshDisplay = true;
      pServer->getAdvertising()->start();
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
  
  // Animasi Bar saat Play (Refresh sedikit lebih sering jika play)
  if (musicState == "play" && deviceConnected) {
    static uint32_t lastAnim = 0;
    if (millis() - lastAnim > 200) { // Animasi berganti tiap 200ms
      refreshDisplay = true;
      lastAnim = millis();
    }
  }
  delay(50);
}