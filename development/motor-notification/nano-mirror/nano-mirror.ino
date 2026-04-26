#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- KONFIGURASI LCD 16X2 ---
// SDA tetap di 23, SCL tetap di 19 sesuai permintaan sebelumnya
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- UUID GADGETBRIDGE ---
#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristicTX;
bool deviceConnected = false;
String artist = "-", title = "-", currentTime = "00:00";
String navInstr = "", navDist = "";
String musicState = "pause";
int volumeLevel = 0;
long lastUnixTime = 0;
String inputBuffer = "";
bool refreshDisplay = true;
bool isNavigating = false;

// Variabel untuk rotasi tampilan
unsigned long lastSwitchTime = 0;
int displayMode = 0; // 0: Musik, 1: Navigasi

void updateDisplay() {
  lcd.clear();

  if (!deviceConnected) {
    lcd.setCursor(0, 0);
    lcd.print("MENUNGGU BLE...");
    lcd.setCursor(0, 1);
    lcd.print("Bangle.js Mode");
    return;
  }

  // Baris 1 selalu Jam dan Volume agar informatif
  lcd.setCursor(0, 0);
  lcd.print(currentTime);
  lcd.setCursor(10, 0);
  lcd.print("V:");
  lcd.print(volumeLevel);
  lcd.print("%");

  // Baris 2: Bergantian antara Musik dan Navigasi
  lcd.setCursor(0, 1);
  if (isNavigating && (displayMode == 1)) {
    // Tampilan Navigasi
    String infoNav = navDist + " " + navInstr;
    lcd.print(infoNav.substring(0, 16));
  } else {
    // Tampilan Musik
    String status = (musicState == "play") ? ">" : "||";
    String infoMusik = status + title;
    lcd.print(infoMusik.substring(0, 16));
  }
}

void processBuffer(String data) {
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    lastUnixTime = data.substring(startIdx, data.indexOf(")")).toInt();
    struct tm *tmp = localtime(&lastUnixTime);
    char buf[10];
    sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
    currentTime = String(buf);
  }

  int start = data.indexOf("GB({");
  int end = data.lastIndexOf("})");
  if (start != -1 && end != -1) {
    String jsonStr = data.substring(start + 3, end + 2);
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, jsonStr) == DeserializationError::Ok) {
      String type = doc["t"] | "";
      if (type == "musicinfo") {
        title = doc["track"] | "No Title";
        artist = doc["artist"] | "No Artist";
      } else if (type == "musicstate") {
        musicState = doc["state"].as<String>();
      } else if (type == "nav") {
        navInstr = doc["instr"] | "";
        navDist = doc["distance"] | "";
        isNavigating = (navInstr != "" && navInstr != " ");
        if (isNavigating) displayMode = 1; // Prioritaskan nav jika baru masuk
      } else if (type == "audio") {
        volumeLevel = doc["v"];
      }
    }
  }
  refreshDisplay = true;
}

void sendToGB(String cmd) {
  if (deviceConnected) {
    pCharacteristicTX->setValue(cmd.c_str());
    pCharacteristicTX->notify();
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
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      refreshDisplay = true;
      delay(500); 
      sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"ESP32-16x2\"})\n");
      sendToGB("GB({\"t\":\"music\",\"n\":\"info\"})\n");
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      refreshDisplay = true;
      pServer->getAdvertising()->start();
    }
};

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi I2C (SDA=23, SCL=19)
  Wire.begin(23, 19);
  
  // Inisialisasi LCD 16x2
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("BOOTING...");

  // Inisialisasi BLE
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
  
  delay(1000);
  refreshDisplay = true;
}

void loop() {
  // Rotasi tampilan setiap 3 detik jika sedang navigasi
  if (isNavigating && millis() - lastSwitchTime > 3000) {
    displayMode = !displayMode;
    lastSwitchTime = millis();
    refreshDisplay = true;
  }

  if (refreshDisplay) { 
    updateDisplay(); 
    refreshDisplay = false; 
  }
  delay(100);
}