#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // GANTI: Dari SH110X ke SSD1306

// Definisikan pin I2C custom untuk Lolin32 Lite
#define I2C_SDA 23
#define I2C_SCL 19

// --- KONFIGURASI OLED SSD1306 ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
#define i2c_Address 0x3C 

// Inisialisasi objek SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- UUID GADGETBRIDGE ---
#define SERVICE_UUID         "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
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

// --- FUNGSI UPDATE TAMPILAN ---
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); // GANTI: Konstanta warna SSD1306

  if (!deviceConnected) {
    display.setTextSize(1);
    display.setCursor(15, 25);
    display.print("WAITING BLE...");
    display.display();
    return;
  }

  // 1. Header: Jam & Volume
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.print(currentTime);
  
  display.setCursor(85, 10);
  display.print("V:"); display.print(volumeLevel); display.print("%");

  // 2. Konten Tengah
  if (isNavigating) {
    display.setCursor(0, 22);
    display.print("NAV: "); display.print(navDist);
    display.setCursor(0, 32);
    display.print(navInstr.substring(0, 20));
  } else {
    display.setCursor(0, 22);
    String shortTitle = title.length() > 18 ? title.substring(0, 16) + ".." : title;
    display.print(shortTitle);
    
    display.setCursor(0, 32);
    String shortArtist = artist.length() > 18 ? artist.substring(0, 16) + ".." : artist;
    display.print(shortArtist);
  }

  // 3. Footer: Status Bar
  display.setCursor(0, 55);
  display.print(musicState == "play" ? "> PLAYING" : "|| PAUSED");

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

  // Inisialisasi I2C dengan pin custom (SDA=23, SCL=19)
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Inisialisasi OLED SSD1306
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, i2c_Address)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Berhenti jika OLED tidak terdeteksi
  }
  
  display.clearDisplay();
  display.display();

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
}

void loop() {
  if (refreshDisplay) { 
    updateDisplay(); 
    refreshDisplay = false; 
  }
  delay(100);
}