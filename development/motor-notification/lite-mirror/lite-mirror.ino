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
#include <Preferences.h> // Pustaka untuk menyimpan data ke Flash Memory ESP32

// --- KONFIGURASI PIN ---
#define I2C_SDA 21
#define I2C_SCL 22
#define TOUCH_PIN 14

// --- KONFIGURASI OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define i2c_Address 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- INSTANSI SENSOR, RTC & PREFERENCES ---
RTC_DS1307 rtc;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
Preferences preferences;

// --- VARIABEL TRIP TIMER ---
unsigned long tripDurationSeconds = 0; 
unsigned long lastTripUpdateTime = 0;  
bool isEngineRunning = false;          
unsigned long lastVibrationTime = 0;   
const unsigned long engineOffTimeout = 5000; 

// Variabel filter & threshold akselerometer
float offsetMag = 0;
float threshold = 0.25;     // Nilai default awal (akan di-overwrite oleh Preferences jika ada)
float filteredAccel = 0;
const float alpha = 0.3;   

// --- VARIABEL SISTEM & BLE ---
int currentMode = 0;  // 0:Musik, 1:Jam, 2:Trip Meter, 3:Navigasi
bool isSettingSensitivity = false; // Status apakah sedang di dalam menu edit sensitivitas
bool deviceConnected = false;
String artist = "-", title = "-", currentTime = "00:00";
String navInstr = "", navDist = "", navEta = "";
String musicState = "pause";
int volumeLevel = 0;
String inputBuffer = "";
bool isNavigating = false;

// --- VARIABEL NOTIFIKASI ---
bool isNotify = false;
String notifySrc = "", notifyTitle = "", notifyBody = "";

// --- VARIABEL PANGGILAN ---
bool isIncomingCall = false;
String callName = "";

// --- VARIABEL OVERLAY VOLUME ---
bool showVolumeOverlay = false;
unsigned long lastVolumeChangeMillis = 0;
int lastVolumeLevel = 0;

// --- VARIABEL RUNNING TEXT ---
int scrollPos = 0;
unsigned long lastScrollTime = 0;
int scrollDelay = 150;        
bool scrollTitleTurn = true;  
unsigned long lastSwitchTime = 0;
int switchDelay = 5000;  

// --- VARIABEL TOUCH ADVANCED ---
unsigned long touchStartTime = 0;
unsigned long lastTouchReleaseTime = 0;
bool isTouching = false;
int touchCounter = 0;
const int longTouchDuration = 800;    
const int doubleTouchInterval = 300;  

// Array nama hari & bulan
const char *namaHari[] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };
const char *namaBulan[] = { "", "Januari", "Februari", "Maret", "April", "Mei", "Juni",
                            "Juli", "Agustus", "September", "Oktober", "November", "Desember" };

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

void drawScrollingText(String text, int y, bool isFocus) {
  int maxChars = 21; 
  if (text.length() <= maxChars) {
    display.setCursor(0, y);
    display.print(text);
  } else {
    if (isFocus) {
      String displayStr = text + "   " + text;  
      int subStart = (scrollPos % (text.length() + 3));
      display.setCursor(0, y);
      display.print(displayStr.substring(subStart, subStart + maxChars));
    } else {
      display.setCursor(0, y);
      display.print(text.substring(0, maxChars - 2) + "..");
    }
  }
}

// --- FUNGSI DETEKSI GETARAN & UPDATE TRIP ---
void processTripTimer() {
  sensors_event_t event;
  accel.getEvent(&event);

  float totalMag = sqrt(sq(event.acceleration.x) + sq(event.acceleration.y) + sq(event.acceleration.z));
  float netAccel = totalMag - offsetMag; 

  filteredAccel = (alpha * netAccel) + ((1 - alpha) * filteredAccel);

  unsigned long now = millis();

  if (abs(filteredAccel) > threshold) {
    lastVibrationTime = now;
    isEngineRunning = true;
  } else {
    if (now - lastVibrationTime > engineOffTimeout) {
      isEngineRunning = false;
    }
  }

  if (isEngineRunning) {
    if (now - lastTripUpdateTime >= 1000) { 
      tripDurationSeconds++;
      lastTripUpdateTime = now;
    }
  } else {
    lastTripUpdateTime = now;
  }
}

// --- FUNGSI UPDATE TAMPILAN OLED ---
void updateDisplay() {
  if (currentMode == 0 && !deviceConnected) {
    currentMode = 1;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // 1. PRIORITAS TERTINGGI: INCOMING CALL
  if (isIncomingCall) {
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.print("[ PANGGILAN ]");  
    display.drawFastHLine(0, 10, 128, WHITE);

    if (millis() - lastScrollTime > scrollDelay) {
      scrollPos++;
      lastScrollTime = millis();
    }

    display.setTextSize(2);
    int maxCharsCall = 10;
    if (callName.length() <= maxCharsCall) {
      int centerCall = (128 - (callName.length() * 12)) / 2;
      display.setCursor(centerCall, 25);  
      display.print(callName);
    } else {
      String displayStr = callName + "   " + callName;
      int subStart = (scrollPos % (callName.length() + 3));
      display.setCursor(0, 25);
      display.print(displayStr.substring(subStart, subStart + maxCharsCall));
    }

    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print("Hold:Reject   Dbl:Accept");
  }
  // 2. PRIORITAS KEDUA: NOTIFIKASI
  else if (isNotify) {
    display.setTextSize(1);
    String srcText = "[ " + notifySrc + " ]";
    int centerSrc = (128 - (srcText.length() * 6)) / 2;
    display.setCursor(centerSrc, 0);
    display.print(srcText);

    if (millis() - lastScrollTime > scrollDelay) {
      scrollPos++;
      lastScrollTime = millis();
    }

    int maxChars = 21;
    if (notifyTitle.length() <= maxChars) {
      int centerTitle = (128 - (notifyTitle.length() * 6)) / 2;
      display.setCursor(centerTitle, 12);
      display.print(notifyTitle);
    } else {
      String displayStr = notifyTitle + "   " + notifyTitle;
      int subStart = (scrollPos % (notifyTitle.length() + 3));
      display.setCursor(0, 12);
      display.print(displayStr.substring(subStart, subStart + maxChars));
    }

    display.drawFastHLine(0, 22, 128, WHITE);
    display.setCursor(0, 28);
    String quotedBody = "\"" + notifyBody + "\"";
    display.print(quotedBody);
  } else if (showVolumeOverlay) {
    display.setTextSize(1);
    display.setCursor(35, 10);
    display.print("VOLUME");
    display.setTextSize(4);
    display.setCursor(30, 25);
    display.print(volumeLevel);
    display.setTextSize(2);
    display.print("%");
    display.drawRect(10, 58, 108, 5, WHITE);
    int volWidth = map(volumeLevel, 0, 100, 0, 108);
    display.fillRect(10, 58, volWidth, 5, WHITE);
  } else {
    updateTimeFromRTC();
    switch (currentMode) {
      case 0: {  // MODE MUSIK
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.print(currentTime);

          if (!deviceConnected) {
            display.setCursor(0, 30);
            display.print("DEVICE DISCONNECTED");
          } else {
            display.setCursor(85, 0);
            display.printf("V:%d%%", volumeLevel);

            if (millis() - lastSwitchTime > switchDelay) {
              scrollTitleTurn = !scrollTitleTurn;
              scrollPos = 0;  
              lastSwitchTime = millis();
            }

            if (millis() - lastScrollTime > scrollDelay) {
              scrollPos++;
              lastScrollTime = millis();
            }

            drawScrollingText(title, 22, scrollTitleTurn);
            drawScrollingText(artist, 35, !scrollTitleTurn);

            display.setCursor(0, 55);
            display.print(musicState == "play" ? "> PLAYING" : "|| PAUSED");
          }
          break;
        }
      case 1: {  // MODE JAM BESAR
          DateTime now = rtc.now();
          display.setTextSize(3);
          display.setCursor(19, 10);
          display.print(currentTime);

          display.setTextSize(1);
          String hari = String(namaHari[now.dayOfTheWeek()]);
          int centerHari = (128 - (hari.length() * 6)) / 2;
          display.setCursor(centerHari, 40);
          display.print(hari);

          String tglPanjang = String(now.day()) + " " + String(namaBulan[now.month()]) + " " + String(now.year());
          int centerTgl = (128 - (tglPanjang.length() * 6)) / 2;
          display.setCursor(centerTgl, 52);
          display.print(tglPanjang);
          break;
        }
      case 2: {  // MODE 2: TRIP METER & SETTING SENSITIVITAS
          if (isSettingSensitivity) {
            // === SUB-MENU: PENGATURAN SENSITIVITAS ===
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.print("SET SENSITIVITY");
            display.drawFastHLine(0, 11, 128, WHITE);

            display.setCursor(0, 16);
            display.print("Engine Vibration Thresh");

            // Tampilkan Nilai Threshold Besar di Tengah
            display.setTextSize(2);
            display.setCursor(40, 32);
            display.print(threshold, 1); // 1 angka di belakang koma

            // Visualisator Bar Horizontal Skala 0.1 - 5.0
            display.drawRect(14, 52, 100, 5, WHITE);
            int barWidth = map(constrain(threshold * 10, 1, 50), 1, 50, 0, 100);
            display.fillRect(14, 52, barWidth, 5, WHITE);

            // Petunjuk navigasi tipis di bawah bar
            display.setTextSize(1);
            display.setCursor(0, 58);
            // display.print("1x:+0.5 | Hold:Save");
          } else {
            // === TAMPILAN NORMAL TRIP METER ===
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.print("TRIP DURATION");
            
            display.setCursor(85, 0);
            display.print(isEngineRunning ? "[RUN]" : "[STP]");

            unsigned long t = tripDurationSeconds;
            int s = t % 60;
            t /= 60;
            int m = t % 60;
            int h = t / 60;

            char tripBuf[10];
            sprintf(tripBuf, "%02d:%02d:%02d", h, m, s);

            display.setTextSize(2);
            display.setCursor(16, 20);
            display.print(tripBuf);

            display.setTextSize(1);
            display.setCursor(0, 44);
            display.printf("Thresh: %.1f | Act: %.1f", threshold, abs(filteredAccel));

            display.setCursor(0, 56);
            display.print("Hold:Reset | Dbl:Setup");
          }
          break;
        }
      case 3: {  // MODE NAVIGASI
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.print("NAVIGASI");
          display.drawFastHLine(0, 10, 128, WHITE);

          display.setCursor(0, 15);
          display.print("Dst: " + navDist);  

          int etaPos = 128 - (navEta.length() * 6);
          display.setCursor(etaPos, 15);
          display.print(navEta);  

          display.setTextSize(1);
          display.setCursor(0, 28);
          display.print(navInstr);
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
    rtc.adjust(DateTime(unixTimeUTC + 28800));  
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
        navEta = doc["eta"] | "";
        String rawDist = doc["distance"] | "";
        navDist = "";
        for (int i = 0; i < rawDist.length(); i++) {
          char c = rawDist[i];
          if (c >= 32 && c <= 126) navDist += c;
          else navDist += " ";
        }
        navDist.trim();
        isNavigating = (navInstr != "" && navInstr != " ");
        if (isNavigating) currentMode = 3;
      } else if (type == "audio") {
        int newVolume = doc["v"];
        if (newVolume != volumeLevel) {
          volumeLevel = newVolume;
          showVolumeOverlay = true;
          lastVolumeChangeMillis = millis();
        }
      } else if (type == "notify") {
        String tempSrc = doc["src"] | "";
        if (tempSrc != "Incoming call") {
          notifySrc = tempSrc;
          notifyTitle = doc["title"] | "";
          notifyBody = doc["body"] | "";
          isNotify = true;
          scrollPos = 0;
        }
      } else if (type == "call") {
        String cmd = doc["cmd"] | "";
        if (cmd == "incoming") {
          callName = doc["name"] | "Unknown";
          isIncomingCall = true;
        } else if (cmd == "end" || cmd == "accept") {
          isIncomingCall = false;
        }
      }
    }
  }
}

void sendToGB(String cmd) {
  if (deviceConnected) {
    String finalPacket = "\x03\x10"+ cmd + "\n";
    pCharacteristicTX->setValue(finalPacket.c_str());
    pCharacteristicTX->notify();
    Serial.print("Sending to Android: ");
    Serial.println(finalPacket);
  }
}

void sendRawJSON(String jsonString) {
  if (deviceConnected) {
    if (!jsonString.endsWith("\n")) {
      jsonString += "\n";
    }
    pCharacteristicTX->setValue(jsonString.c_str());
    pCharacteristicTX->notify();
    Serial.print("Raw Sent: ");
    Serial.print(jsonString);
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
    delay(500);
    sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"Lolin32-Lite\"})\n");
    sendToGB("GB({\"t\":\"music\",\"n\":\"info\"})\n");
  }
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    isNavigating = false;
    pServer->getAdvertising()->start();
  }
};

void checkSerial() {
  if (Serial.available()) {
    String manualCmd = Serial.readStringUntil('\n');
    manualCmd.trim();  
    if (manualCmd.startsWith("GB({")) {
      sendToGB(manualCmd);
    } else {
      sendRawJSON(manualCmd);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) Serial.println("RTC Gagal!");
  if (!accel.begin()) Serial.println("ADXL345 Gagal!");
  if (!display.begin(SSD1306_SWITCHCAPVCC, i2c_Address))
    for (;;);

  // Ambil data Simpanan Sensitivitas dari Flash Memory ESP32
  preferences.begin("dashboard", false);
  threshold = preferences.getFloat("threshold", 0.25); // Default 0.25 jika memori kosong
  preferences.end();

  // Kalibrasi ADXL345
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  display.println("KALIBRASI SENSOR...");
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
  BLEDevice::setMTU(512);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pService->start();
  pServer->getAdvertising()->start();

  lastTripUpdateTime = millis();
  lastVibrationTime = millis();
  Serial.println("Sistem Siap!");
}

void loop() {
  checkSerial();
  
  // --- 1. LOGIKA TOUCH SENSOR (SINGLE, DOUBLE, LONG) ---
  bool currentTouch = digitalRead(TOUCH_PIN);

  if (currentTouch == HIGH && !isTouching) {
    isTouching = true;
    touchStartTime = millis();
  }

  if (currentTouch == LOW && isTouching) {
    isTouching = false;
    unsigned long duration = millis() - touchStartTime;

    if (duration >= longTouchDuration) {
      // === ACTION: LONG TOUCH ===
      if (isIncomingCall) {
        sendToGB("GB({\"t\":\"call\",\"cmd\":\"reject\"})\n");
        isIncomingCall = false;
      } else if (isNotify) {
        isNotify = false;
      } else if (currentMode == 2) {
        if (isSettingSensitivity) {
          // Jika sedang di menu setting, Long Touch berarti SIMPAN & KELUAR
          preferences.begin("dashboard", false);
          preferences.putFloat("threshold", threshold);
          preferences.end();
          isSettingSensitivity = false; 
          Serial.println("Action: Long Touch - Save Threshold & Exit");
        } else {
          // Jika di tampilan trip normal, Long Touch berarti RESET JAM TRIP
          tripDurationSeconds = 0;
          Serial.println("Action: Long Touch - Reset Trip Timer");
        }
      } else {
        sendToGB("GB({\"t\":\"music\",\"n\":\"toggle\"})\n");
      }
      touchCounter = 0;
    } else {
      touchCounter++;
      lastTouchReleaseTime = millis();
    }
  }

  if (touchCounter > 0 && (millis() - lastTouchReleaseTime > doubleTouchInterval)) {
    if (touchCounter == 1) {
      // === ACTION: SINGLE TOUCH ===
      if (isIncomingCall || isNotify) {
        isIncomingCall = false;
        isNotify = false;
      } else if (currentMode == 2 && isSettingSensitivity) {
        // Jika sedang di dalam menu setting sensitivitas:
        // Naikkan kelipatan 0.5, batas max 5.0, balik ke 0.1 jika lewat.
        threshold += 0.5;
        if (threshold > 5.0) {
          threshold = 0.1;
        }
        Serial.printf("Action: Single Touch - Threshold set to %.1f\n", threshold);
      } else {
        // Jika di menu normal lainnya, ganti mode seperti biasa
        currentMode++;
        if (currentMode == 0 && !deviceConnected) currentMode = 1;
        if (!isNavigating && currentMode == 3) currentMode = 0;
        else if (currentMode > 3) currentMode = 0;
        if (currentMode == 0 && !deviceConnected) currentMode = 1;
        Serial.println("Action: Single Touch - Next Mode");
      }
    } else if (touchCounter >= 2) {
      // === ACTION: DOUBLE TOUCH ===
      if (isIncomingCall) {
        sendToGB("GB({\"t\":\"call\",\"cmd\":\"accept\"})\n");
        isIncomingCall = false;
      } else if (currentMode == 2) {
        // KHUSUS MODE 2: Double Touch untuk masuk/keluar Menu Sensitivitas
        isSettingSensitivity = !isSettingSensitivity;
        Serial.print("Action: Double Touch - Sensitivity Menu ");
        Serial.println(isSettingSensitivity ? "ENTER" : "EXIT WITHOUT SAVE");
      } else if (!isNotify) {
        sendToGB("GB({\"t\":\"music\",\"n\":\"next\"})\n");
      }
    }
    touchCounter = 0; 
  }

  // --- 2. LOGIKA OVERLAY VOLUME (Timeout 3 Detik) ---
  if (showVolumeOverlay && (millis() - lastVolumeChangeMillis > 3000)) {
    showVolumeOverlay = false;
  }

  // --- 3. BACKGROUND PROCESS (DETEKSI GETARAN & TRIP) ---
  processTripTimer();

  // --- 4. UPDATE LAYAR (Setiap 100ms agar smooth) ---
  static unsigned long lastDisplayMillis = 0;
  if (millis() - lastDisplayMillis > 100) {
    updateDisplay();
    lastDisplayMillis = millis();
  }
}