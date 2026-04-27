#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "RTClib.h"

// --- KONFIGURASI PIN ---
#define I2C_SDA 23
#define I2C_SCL 19
#define TOUCH_PIN 14

// --- KONFIGURASI OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define i2c_Address 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- INSTANSI SENSOR & RTC ---
RTC_DS1307 rtc;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// --- VARIABEL SPEDOMETER ---
float speedKMH = 0;
float velocityMS = 0;
unsigned long lastAccelTime = 0;
float offsetMag = 0;
float threshold = 0.6;

// --- VARIABEL SISTEM & BLE ---
int currentMode = 0;  // 0:Musik, 1:Jam, 2:Spedometer
bool deviceConnected = false;
String artist = "-", title = "-", currentTime = "00:00";
String navInstr = "", navDist = "";
String musicState = "pause";
int volumeLevel = 0;
String inputBuffer = "";
bool isNavigating = false;

// --- UUID GADGETBRIDGE ---
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
BLECharacteristic *pCharacteristicTX;

// --- FUNGSI RTC ---
void updateTimeFromRTC() {
  DateTime now = rtc.now();
  char buf[10];
  sprintf(buf, "%02d:%02d", now.hour(), now.minute());
  currentTime = String(buf);
}

// --- FUNGSI PERHITUNGAN SPEDOMETER ---
void processAccel() {
  sensors_event_t event;
  accel.getEvent(&event);

  unsigned long now = millis();
  float dt = (now - lastAccelTime) / 1000.0;
  lastAccelTime = now;

  float currentMag = sqrt(sq(event.acceleration.x) + sq(event.acceleration.y) + sq(event.acceleration.z));
  float linearAcc = currentMag - offsetMag;

  if (abs(linearAcc) < threshold) linearAcc = 0;

  // Integrasi percepatan menjadi kecepatan
  velocityMS += linearAcc * dt;
  if (velocityMS < 0) velocityMS = 0;

  // Auto-reset jika diam (mencegah drift)
  static unsigned long lastMoveTime = 0;
  if (abs(linearAcc) > 0.1) {
    lastMoveTime = millis();
  } else if (millis() - lastMoveTime > 1500) {
    velocityMS *= 0.9;
    if (velocityMS < 0.1) velocityMS = 0;
  }
  speedKMH = velocityMS * 3.6;
}

// --- FUNGSI UPDATE TAMPILAN OLED ---
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  updateTimeFromRTC();

  if (isNavigating) {
    display.setTextSize(1);
    display.setCursor(0, 5); display.print("NAVIGASI AKTIF");
    display.drawFastHLine(0, 15, 128, WHITE);
    display.setCursor(0, 25); display.print(navDist);
    display.setTextSize(2);
    display.setCursor(0, 40); display.print(navInstr.substring(0, 10));
  } 
  else {
    switch (currentMode) {
      case 0: { // MODE MUSIK
        display.setTextSize(1);
        display.setCursor(0, 0); display.print(currentTime);
        display.setCursor(85, 0); display.printf("V:%d%%", volumeLevel);
        display.setCursor(0, 22); display.print(title.length() > 18 ? title.substring(0, 16) + ".." : title);
        display.setCursor(0, 35); display.print(artist.length() > 18 ? artist.substring(0, 16) + ".." : artist);
        display.setCursor(0, 55); display.print(musicState == "play" ? "> PLAYING" : "|| PAUSED");
        break;
      }

      case 1: { // MODE JAM BESAR
        display.setTextSize(3);
        display.setCursor(19, 15); display.print(currentTime);
        display.setTextSize(1);
        DateTime now = rtc.now(); // Variabel ini sekarang aman di dalam scope kurung kurawal
        display.setCursor(34, 48);
        display.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());
        break;
      }

      case 2: { // MODE SPEDOMETER
        display.setTextSize(1);
        display.setCursor(0, 0); display.print("SPEEDOMETER");
        display.setTextSize(4);
        display.setCursor(20, 20); display.print((int)speedKMH);
        display.setTextSize(1);
        display.print(" km/h");
        
        display.drawRect(0, 60, 128, 4, WHITE);
        int barWidth = map(constrain((int)speedKMH, 0, 60), 0, 60, 0, 128);
        display.fillRect(0, 60, barWidth, 4, WHITE);
        break;
      }
    }
  }
  display.display();
}

// --- FUNGSI TERIMA DATA GADGETBRIDGE ---
void processBuffer(String data) {
  Serial.print("Data Masuk: ");
  Serial.println(data);

  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    long unixTimeUTC = data.substring(startIdx, data.indexOf(")")).toInt();
    rtc.adjust(DateTime(unixTimeUTC + 28800));  // Sync GMT+8
    Serial.println("RTC Synced (WITA)");
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
}

void sendToGB(String cmd) {
  if (deviceConnected) {
    pCharacteristicTX->setValue(cmd.c_str());
    pCharacteristicTX->notify();
  }
}

// --- CALLBACK BLE ---
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    for (char c : value) {
      if (c == '\n') {
        processBuffer(inputBuffer);
        inputBuffer = "";
      } else {
        inputBuffer += c;
      }
    }
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    delay(500);
    sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"Lolin32-Lite\"})\n");
    sendToGB("GB({\"t\":\"music\",\"n\":\"info\"})\n");
  }
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    pServer->getAdvertising()->start();
  }
};

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) Serial.println("RTC Gagal!");
  if (!accel.begin()) Serial.println("ADXL345 Gagal!");
  if (!display.begin(SSD1306_SWITCHCAPVCC, i2c_Address))
    for (;;)
      ;

  // Kalibrasi ADXL345 (Pastikan alat diam)
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("KALIBRASI ADXL...");
  display.display();

  float sumMag = 0;
  for (int i = 0; i < 50; i++) {
    sensors_event_t event;
    accel.getEvent(&event);
    sumMag += sqrt(sq(event.acceleration.x) + sq(event.acceleration.y) + sq(event.acceleration.z));
    delay(20);
  }
  offsetMag = sumMag / 50.0;

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

  lastAccelTime = millis();
  Serial.println("Sistem Siap!");
}

// --- LOOP UTAMA ---
void loop() {
  // 1. Cek Touch Sensor untuk Ganti Mode
  static bool lastTouchState = LOW;
  bool currentTouch = digitalRead(TOUCH_PIN);
  if (currentTouch == HIGH && lastTouchState == LOW) {
    currentMode++;
    if (currentMode > 2) currentMode = 0;
    Serial.print("Mode Sekarang: ");
    Serial.println(currentMode);
    delay(200);  // Debounce
  }
  lastTouchState = currentTouch;

  // 2. Update Perhitungan Spedometer (Background)
  processAccel();

  // 3. Update Layar secara berkala
  static unsigned long lastDisplayMillis = 0;
  if (millis() - lastDisplayMillis > 100) {  // Update setiap 100ms
    updateDisplay();
    lastDisplayMillis = millis();
  }
}