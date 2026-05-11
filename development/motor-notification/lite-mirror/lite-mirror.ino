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
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define OTA_JUMPER_PIN 23
bool otaMode = false;  // Flag untuk mengecek apakah OTA aktif

// --- KONFIGURASI PIN ---
#define I2C_SDA 21
#define I2C_SCL 22
#define TOUCH_PIN 14

// Konfigurasi WiFi (Wajib untuk OTA)
const char *ssid = "MIKRO";
const char *password = "1DEAlist";

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

// --- VARIABEL NOTIFIKASI ---
bool isNotify = false;
String notifySrc = "", notifyTitle = "", notifyBody = "";

// --- VARIABEL OVERLAY VOLUME ---
bool showVolumeOverlay = false;
unsigned long lastVolumeChangeMillis = 0;
int lastVolumeLevel = 0;  // Untuk mendeteksi perubahan

// Array nama hari dalam Bahasa Indonesia
const char *namaHari[] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };

// --- UUID GADGETBRIDGE ---
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
BLECharacteristic *pCharacteristicTX;

void setupOTA() {
  pinMode(OTA_JUMPER_PIN, INPUT_PULLUP);  // Menggunakan internal pull-up

  // Cek jika pin 23 dijumper ke GND (LOW)
  if (digitalRead(OTA_JUMPER_PIN) == LOW) {
    otaMode = true;

    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("OTA MODE ACTIVE");
    display.println("Connecting WiFi...");
    display.display();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Tunggu koneksi maksimal 10 detik agar tidak stuck
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.setHostname("Smartwatch-ESP32");
      ArduinoOTA.onStart([]() {
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("OTA UPDATING...");
        display.display();
      });
      ArduinoOTA.begin();
      Serial.println("\nOTA Ready!");
    } else {
      display.println("WiFi Timeout!");
      display.display();
      delay(2000);
      otaMode = false;
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
  } else {
    otaMode = false;
    Serial.println("OTA Mode Disabled (No Jumper)");
  }
}

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

  // 1. PRIORITAS UTAMA: NOTIFIKASI
  if (isNotify) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("[" + notifySrc + "]");
    display.drawFastHLine(0, 11, 128, WHITE);
    display.setCursor(0, 15);
    display.print(notifyTitle);
    display.setCursor(0, 30);
    display.setTextSize(1);
    display.print(notifyBody.length() > 20 ? notifyBody.substring(0, 18) + ".." : notifyBody);
    display.setTextSize(1);
    display.setCursor(20, 56);
    display.print("> Dismiss <");
  }
  // 2. PRIORITAS KEDUA: VOLUME OVERLAY
  else if (showVolumeOverlay) {
    display.setTextSize(1);
    display.setCursor(35, 10);
    display.print("VOLUME");

    display.setTextSize(4);
    display.setCursor(30, 25);
    display.print(volumeLevel);
    display.setTextSize(2);
    display.print("%");

    // Progress Bar Volume di bagian bawah
    display.drawRect(10, 58, 108, 5, WHITE);
    int volWidth = map(volumeLevel, 0, 100, 0, 108);
    display.fillRect(10, 58, volWidth, 5, WHITE);
  } else {
    updateTimeFromRTC();
    switch (currentMode) {
      case 0:
        {  // MODE MUSIK
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.print(currentTime);
          if (!deviceConnected) {
            display.setCursor(0, 30);
            display.setTextSize(1);
            display.print("DEVCE DISCONNECTED");
          } else {
            display.setCursor(85, 0);
            display.printf("V:%d%%", volumeLevel);
            display.setCursor(0, 22);
            display.print(title.length() > 18 ? title.substring(0, 16) + ".." : title);
            display.setCursor(0, 35);
            display.print(artist.length() > 18 ? artist.substring(0, 16) + ".." : artist);
            display.setCursor(0, 55);
            display.print(musicState == "play" ? "> PLAYING" : "|| PAUSED");
          }
          break;
        }
      case 1:
        {                            // MODE JAM BESAR
          DateTime now = rtc.now();  // Ambil data waktu terbaru

          // Tampilkan Jam Besar
          display.setTextSize(3);
          display.setCursor(19, 10);
          display.print(currentTime);

          // Tampilkan Nama Hari (di bawah jam)
          display.setTextSize(1);
          int centerX = (128 - (String(namaHari[now.dayOfTheWeek()]).length() * 6)) / 2;  // Hitung posisi tengah
          display.setCursor(centerX, 40);
          display.print(namaHari[now.dayOfTheWeek()]);

          // Tampilkan Tanggal (paling bawah)
          display.setCursor(34, 52);
          display.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());
          break;
        }
      case 2:
        {  // MODE SPEDOMETER
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.print("SPEEDOMETER");
          display.setTextSize(4);
          display.setCursor(20, 20);
          display.print((int)speedKMH);
          display.setTextSize(2);
          display.print(" km/h");
          display.drawRect(0, 60, 128, 4, WHITE);
          int barWidth = map(constrain((int)speedKMH, 0, 60), 0, 60, 0, 128);
          display.fillRect(0, 60, barWidth, 4, WHITE);
          break;
        }
      case 3:
        {  // MODE NAVIGASI (Hanya jika aktif)
          display.setTextSize(1);
          display.setCursor(0, 5);
          display.print("NAVIGASI");
          display.drawFastHLine(0, 15, 128, WHITE);
          display.setCursor(0, 25);
          display.print(navDist);
          display.setTextSize(2);
          // Menampilkan instruksi lebih jelas
          display.setCursor(0, 40);
          display.print(navInstr.length() > 10 ? navInstr.substring(0, 10) : navInstr);
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
        String rawDist = doc["distance"] | "";  // Ambil data mentah jarak

        // --- PERBAIKAN KARAKTER ANEH ---
        // Kita akan membersihkan karakter non-ASCII (seperti )
        navDist = "";
        for (int i = 0; i < rawDist.length(); i++) {
          char c = rawDist[i];
          // Hanya ambil karakter angka, huruf, dan spasi (ASCII 32-126)
          if (c >= 32 && c <= 126) {
            navDist += c;
          } else {
            navDist += " ";  // Ganti karakter aneh dengan spasi
          }
        }

        // Bersihkan spasi ganda yang mungkin muncul
        navDist.trim();

        isNavigating = (navInstr != "" && navInstr != " ");
        if (isNavigating) currentMode = 3;
      } else if (type == "audio") {
        int newVolume = doc["v"];
        if (newVolume != volumeLevel) {  // Jika volume berubah
          volumeLevel = newVolume;
          showVolumeOverlay = true;           // Aktifkan overlay
          lastVolumeChangeMillis = millis();  // Reset timer 3 detik
        }
      } else if (type == "notify") {
        notifySrc = doc["src"] | "Notification";
        notifyTitle = doc["title"] | "";
        notifyBody = doc["body"] | "";
        isNotify = true;  // Langsung aktifkan layar notifikasi
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
    isNavigating = false;  // Reset navigasi saat putus
    pServer->getAdvertising()->start();
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(OTA_JUMPER_PIN, INPUT_PULLUP);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(I2C_SDA, I2C_SCL);

  // Inisialisasi Hardware Dasar
  if (!rtc.begin()) Serial.println("RTC Gagal!");
  if (!accel.begin()) Serial.println("ADXL345 Gagal!");
  if (!display.begin(SSD1306_SWITCHCAPVCC, i2c_Address))
    for (;;)
      ;

  // 1. CEK JUMPER OTA TERLEBIH DAHULU
  setupOTA();

  // 2. JIKA OTA AKTIF, BERHENTI DI SINI (Fokus OTA)
  if (otaMode) {
    Serial.println("System focus to OTA. Bluetooth disabled.");
    return;  // Keluar dari setup, fungsi setup selanjutnya (BLE) tidak dijalankan
  }

  // 3. JIKA TIDAK OTA, JALANKAN BLUETOOTH & KALIBRASI (Normal Mode)
  // Kalibrasi ADXL345
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("NORMAL MODE...");
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
  Serial.println("Sistem Siap (Normal Mode)!");
}

// --- LOOP UTAMA ---
void loop() {
  if (otaMode) {
    // FOKUS HANYA OTA
    ArduinoOTA.handle();

    // Opsional: Tampilkan status di layar agar tahu ESP32 tidak hang
    static unsigned long lastTick = 0;
    if (millis() - lastTick > 1000) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 10);
      display.println(">> OTA MODE ON <<");
      display.println("IP: " + WiFi.localIP().toString());
      display.setCursor(0, 40);
      display.println("Waiting for upload...");
      display.display();
      lastTick = millis();
    }
    return;  // "Skip" semua kodingan di bawah (Sensor, Spedo, BLE, dll)
  }
  // 1. Cek Touch Sensor
  static bool lastTouchState = LOW;
  bool currentTouch = digitalRead(TOUCH_PIN);

  if (currentTouch == HIGH && lastTouchState == LOW) {
    if (isNotify) {
      // Jika sedang ada notifikasi, tutup notifikasinya saja
      isNotify = false;
    } else {
      // Jika normal, ganti mode seperti biasa
      currentMode++;
      if (!isNavigating && currentMode == 3) {
        currentMode = 0;
      } else if (currentMode > 3) {
        currentMode = 0;
      }
    }
    delay(200);  // Debounce
  }
  lastTouchState = currentTouch;

  // Cek Timer Volume (3 detik)
  if (showVolumeOverlay && (millis() - lastVolumeChangeMillis > 3000)) {
    showVolumeOverlay = false;
  }

  // 2. Background Process
  processAccel();

  // 3. Update Layar
  static unsigned long lastDisplayMillis = 0;
  if (millis() - lastDisplayMillis > 100) {
    updateDisplay();
    lastDisplayMillis = millis();
  }
}