#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>  // GANTI: Dari SH110X ke SSD1306
#include "RTClib.h"

// Definisikan pin I2C custom untuk Lolin32 Lite
#define I2C_SDA 23
#define I2C_SCL 19

// --- KONFIGURASI OLED SSD1306 ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define i2c_Address 0x3C

// Inisialisasi objek SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Inisialisasi RTC
RTC_DS1307 rtc;

// --- UUID GADGETBRIDGE ---
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
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

#define TOUCH_PIN 14

unsigned long lastActivityTime = 0;
const long standbyDelay = 15000;  // 15 detik
bool isStandby = false;

void updateTimeFromRTC() {
  DateTime now = rtc.now();
  char buf[10];
  sprintf(buf, "%02d:%02d", now.hour(), now.minute());
  currentTime = String(buf);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!deviceConnected && !isStandby) {
    display.setTextSize(1);
    display.setCursor(15, 25);
    display.print("WAITING BLE...");
    display.display();
    return;
  }

  // Update waktu dari RTC setiap kali display refresh
  updateTimeFromRTC();

  if (isNavigating) {
    display.setTextSize(1);
    display.setCursor(0, 22);
    display.print("NAV: ");
    display.print(navDist);
    display.setCursor(0, 32);
    display.print(navInstr.substring(0, 20));
  } else if (isStandby) {
    // 1. TAMPILAN JAM BESAR
    updateTimeFromRTC();  // Pastikan waktu terbaru diambil
    display.setTextSize(3);
    display.setCursor(19, 15);  // Geser sedikit ke atas (y: 15) untuk memberi ruang tanggal
    display.print(currentTime);

    // 2. TAMPILAN TANGGAL
    DateTime now = rtc.now();
    char dateBuf[12];
    // Format: DD/MM/YYYY
    sprintf(dateBuf, "%02d/%02d/%04d", now.day(), now.month(), now.year());

    display.setTextSize(1);
    // Hitung posisi tengah: (128 - (lebar teks)) / 2
    // Lebar teks size 1 (6px per char), 10 char = 60px. (128-60)/2 = 34
    display.setCursor(34, 45);
    display.print(dateBuf);
  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(currentTime);
    display.setCursor(85, 0);
    display.print("V:");
    display.print(volumeLevel);
    display.print("%");

    display.setCursor(0, 22);
    display.print(title.length() > 18 ? title.substring(0, 16) + ".." : title);
    display.setCursor(0, 32);
    display.print(artist.length() > 18 ? artist.substring(0, 16) + ".." : artist);
    display.setCursor(0, 55);
    display.print(musicState == "play" ? "> PLAYING" : "|| PAUSED");
  }
  display.display();
}

void processBuffer(String data) {
  // Tampilkan seluruh data mentah yang diterima ke Serial Monitor
  Serial.print("Data Masuk: ");
  Serial.println(data);

  lastActivityTime = millis();
  isStandby = false;

  // LOGIKA BARU: Set Waktu RTC dari Gadgetbridge
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    // Ambil timestamp mentah (UTC) dari HP
    long unixTimeUTC = data.substring(startIdx, data.indexOf(")")).toInt();

    // Tambahkan 8 jam (28800 detik) untuk GMT+8
    long unixTimeWITA = unixTimeUTC + 28800;

    // Set waktu yang sudah dikoreksi ke modul DS1307
    rtc.adjust(DateTime(unixTimeWITA));

    Serial.print("Waktu sinkron (UTC): ");
    Serial.println(unixTimeUTC);
    Serial.print("Waktu sinkron (WITA): ");
    Serial.println(unixTimeWITA);
  }

  // Proses JSON Gadgetbridge
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
    refreshDisplay = true;
    delay(500);
    sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"Lolin32-Lite\"})\n");
    sendToGB("GB({\"t\":\"music\",\"n\":\"info\"})\n");
  }
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    refreshDisplay = true;
    pServer->getAdvertising()->start();
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- SISTEM DIMULAI ---");  // Tambahkan ini

  pinMode(TOUCH_PIN, INPUT);

  // Inisialisasi I2C dengan pin custom (SDA=23, SCL=19)
  Wire.begin(I2C_SDA, I2C_SCL);

  // Inisialisasi RTC
  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");
  }
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Opsional: set awal pakai waktu compile

  // Inisialisasi OLED SSD1306
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, i2c_Address)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Berhenti jika OLED tidak terdeteksi
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
  if (digitalRead(TOUCH_PIN) == HIGH) {
    if (isStandby) {
      isStandby = false;
      lastActivityTime = millis();
      refreshDisplay = true;
      delay(200);
    }
  }

  if (!isStandby && (millis() - lastActivityTime > standbyDelay)) {
    isStandby = true;
    refreshDisplay = true;
  }

  // Refresh display setiap 1 menit agar jam update meski standby
  static unsigned long lastMinuteUpdate = 0;
  if (millis() - lastMinuteUpdate > 60000) {
    lastMinuteUpdate = millis();
    refreshDisplay = true;
  }

  if (refreshDisplay) {
    updateDisplay();
    refreshDisplay = false;
  }
  delay(50);
}