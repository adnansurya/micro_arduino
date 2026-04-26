#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>

// --- KONFIGURASI PIN LOLIN32 LITE ---
// SDA: 23, SCL: 19 (Pin standar Lolin32 Lite untuk I2C)
#define OLED_RESET -1
Adafruit_SH1106 display(23, 19); 

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
String inputBuffer = "";
bool refreshDisplay = true;
bool isNavigating = false;

// --- FUNGSI TAMPILAN (OLED 128x64) ---
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (!deviceConnected) {
    display.setTextSize(1);
    display.setCursor(20, 25);
    display.print("WAITING FOR BLE...");
    display.display();
    return;
  }

  // 1. Header: Jam & Volume (Baris Atas)
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(currentTime);
  
  display.setTextSize(1);
  display.setCursor(85, 0);
  display.print("V:"); display.print(volumeLevel); display.print("%");
  display.drawFastHLine(0, 17, 128, WHITE);

  // 2. Konten Tengah: Prioritas Navigasi jika aktif
  if (isNavigating) {
    display.setCursor(0, 22);
    display.print("NAV: "); display.print(navDist);
    display.setCursor(0, 32);
    String shortNav = navInstr.substring(0, 20);
    display.print(shortNav);
  } else {
    // Jika tidak navigasi, tampilkan info musik
    display.setCursor(0, 22);
    display.print("M: "); 
    String shortTitle = title.length() > 18 ? title.substring(0, 16) + ".." : title;
    display.print(shortTitle);
    
    display.setCursor(0, 32);
    String shortArtist = artist.length() > 18 ? artist.substring(0, 16) + ".." : artist;
    display.print(shortArtist);
  }

  // 3. Footer: Status Bar
  display.drawFastHLine(0, 50, 128, WHITE);
  display.setCursor(0, 55);
  display.print(musicState == "play" ? "> PLAYING" : "|| PAUSED");
  
  // Progress bar volume kecil di pojok bawah
  int volWidth = map(volumeLevel, 0, 100, 0, 50);
  display.drawRect(75, 55, 50, 7, WHITE);
  display.fillRect(75, 55, volWidth, 7, WHITE);

  display.display();
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

// --- BLE CALLBACKS ---
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
      sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"Lolin32-Lite\"})\n");
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
  
  // Inisialisasi OLED SH1106
  display.begin(SH1106_SWITCHCAPVCC, 0x3C); 
  display.clearDisplay();
  display.display();

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
}

void loop() {
  if (refreshDisplay) { 
    updateDisplay(); 
    refreshDisplay = false; 
  }
  delay(200);
}