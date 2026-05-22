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
#define I2C_SDA 21
#define I2C_SCL 22
#define TOUCH_PIN 14

// --- KONFIGURASI OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define i2c_Address 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- INSTANSI SENSOR & RTC ---
RTC_DS1307 rtc;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// --- VARIABEL TRIP TIMER (PENGGANTI SPEEDOMETER) ---
unsigned long tripDurationSeconds = 0; // Total durasi trip dalam detik
unsigned long lastTripUpdateTime = 0;  // Waktu terakhir durasi trip bertambah
bool isEngineRunning = false;          // Status mesin (bergetar atau tidak)
unsigned long lastVibrationTime = 0;   // Waktu terakhir getaran terdeteksi
const unsigned long engineOffTimeout = 5000; // Motor dianggap mati jika diam selama 5 detik

// Variabel filter akselerometer
float offsetMag = 0;
float threshold = 0.25;     // Sesuaikan nilai ini untuk sensitivitas getaran mesin motor Anda
float filteredAccel = 0;
const float alpha = 0.3;   // Filter sedikit lebih responsif untuk deteksi getaran raw

// --- VARIABEL SISTEM & BLE ---
int currentMode = 0;  // 0:Musik, 1:Jam, 2:Trip Meter, 3:Navigasi
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

  // 1. Hitung Resultan Magnitudo G-Force
  float totalMag = sqrt(sq(event.acceleration.x) + sq(event.acceleration.y) + sq(event.acceleration.z));
  float netAccel = totalMag - offsetMag; 

  // 2. Filter Sederhana (Low Pass Filter)
  filteredAccel = (alpha * netAccel) + ((1 - alpha) * filteredAccel);

  unsigned long now = millis();

  // 3. Cek apakah getaran melebihi threshold (Mesin Menyala/Motor Jalan)
  if (abs(filteredAccel) > threshold) {
    lastVibrationTime = now;
    isEngineRunning = true;
  } else {
    // Jika tidak ada getaran dalam waktu 'engineOffTimeout' (5 detik), mesin dianggap mati
    if (now - lastVibrationTime > engineOffTimeout) {
      isEngineRunning = false;
    }
  }

  // 4. Hitung Waktu Trip (Hanya bertambah jika mesin menyala)
  if (isEngineRunning) {
    if (now - lastTripUpdateTime >= 1000) { // Update setiap 1 detik
      tripDurationSeconds++;
      lastTripUpdateTime = now;
    }
  } else {
    // Jika mesin mati, pastikan anchor penambahan waktu tetap sinkron saat mesin menyala lagi
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
      case 2: {  // MODE PENGGANTI: TRIP DURATION TIMER
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.print("TRIP DURATION");
          
          // Indikator status mesin (Running / Stop) di pojok kanan atas
          display.setCursor(85, 0);
          display.print(isEngineRunning ? "[RUN]" : "[STP]");

          // Konversi detik menjadi format HH:MM:SS
          unsigned long t = tripDurationSeconds;
          int s = t % 60;
          t /= 60;
          int m = t % 60;
          int h = t / 60;

          char tripBuf[10];
          sprintf(tripBuf, "%02d:%02d:%02d", h, m, s);

          // Tampilkan Waktu Trip ukuran besar di tengah
          display.setTextSize(2);
          display.setCursor(16, 22);
          display.print(tripBuf);

          // Petunjuk Reset di bagian bawah
          display.setTextSize(1);
          display.setCursor(10, 50);
          display.print("Hold Touch to Reset");
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
    rtc.adjust(DateTime(unixTimeUTC + 28800));  // Sync GMT+8
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
      // ACTION: LONG TOUCH
      if (isIncomingCall) {
        sendToGB("GB({\"t\":\"call\",\"cmd\":\"reject\"})\n");
        isIncomingCall = false;
        Serial.println("Action: Long Touch - Reject Call");
      } else if (isNotify) {
        isNotify = false;
        Serial.println("Action: Long Touch - Dismiss Notify");
      } else if (currentMode == 2) {
        // KHUSUS MODE 2: Reset Waktu Trip ke 00:00:00 jika di-hold
        tripDurationSeconds = 0;
        Serial.println("Action: Long Touch - Reset Trip Timer");
      } else {
        sendToGB("GB({\"t\":\"music\",\"n\":\"toggle\"})\n");
        Serial.println("Action: Long Touch - Toggle Play/Pause");
      }
      touchCounter = 0;
    } else {
      touchCounter++;
      lastTouchReleaseTime = millis();
    }
  }

  if (touchCounter > 0 && (millis() - lastTouchReleaseTime > doubleTouchInterval)) {
    if (touchCounter == 1) {
      // ACTION: SINGLE TOUCH
      if (isIncomingCall || isNotify) {
        isIncomingCall = false;
        isNotify = false;
      } else {
        currentMode++;
        if (currentMode == 0 && !deviceConnected) currentMode = 1;
        if (!isNavigating && currentMode == 3) currentMode = 0;
        else if (currentMode > 3) currentMode = 0;
        if (currentMode == 0 && !deviceConnected) currentMode = 1;
        Serial.println("Action: Single Touch - Next Mode");
      }
    } else if (touchCounter >= 2) {
      // ACTION: DOUBLE TOUCH
      if (isIncomingCall) {
        sendToGB("GB({\"t\":\"call\",\"cmd\":\"accept\"})\n");
        isIncomingCall = false;
        Serial.println("Action: Double Touch - Accept Call");
      } else if (!isNotify) {
        sendToGB("GB({\"t\":\"music\",\"n\":\"next\"})\n");
        Serial.println("Action: Double Touch - Next Track");
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