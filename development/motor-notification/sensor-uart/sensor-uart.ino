#include <Wire.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// ========================================================
// PARAMETER GETARAN
// ========================================================
float THRESH = 12.0;       
int DET_WIN = 2000;    
int MIN_VIB = 10;      

bool engOn = false;
unsigned long tripStart = 0;
unsigned long tripAcc = 0;
unsigned long lastSend = 0;
String bufUART = "";
bool cfgRecv = false;

unsigned long lastRaw = 0; 
const unsigned long UART_PAUSE = 3000;
const int LED_PIN = 17;

// Debounce engine OFF
unsigned long lastVib = 0;
const unsigned long OFF_DELAY = 10000;
bool pendingOff = false;

// ========================================================
// PROTOKOL KOMUNIKASI DENGAN CYD
// ========================================================
#define CMD_PREFIX "CMD:"      // Prefix untuk perintah dari CYD
#define DATA_PREFIX "DATA:"    // Prefix untuk data ke CYD
#define CFG_PREFIX "CFG:"      // Prefix untuk konfigurasi

String inputBuffer = "";

// ========================================================
// FUNGSI KOMUNIKASI DENGAN CYD VIA Serial1
// ========================================================

// Kirim data ke CYD dengan prefix DATA:
void sendToCYD(String data) {
  Serial1.print(DATA_PREFIX);
  Serial1.println(data);
  Serial1.flush();
  
  // Log ke Serial Monitor (debugging)
  Serial.print("[CYD<-] ");
  Serial.println(data);
}

// Kirim konfigurasi berhasil ke CYD
void sendConfigAckToCYD() {
  StaticJsonDocument<128> doc;
  doc["status"] = "ok";
  doc["threshold"] = THRESH;
  doc["detection_window"] = DET_WIN;
  doc["min_vibration"] = MIN_VIB;
  
  String output;
  serializeJson(doc, output);
  
  Serial1.print(CFG_PREFIX);
  Serial1.println(output);
  Serial1.flush();
  
  Serial.print("[CFG ACK] ");
  Serial.println(output);
}

// Proses perintah dari CYD
void processCommandFromCYD(String command) {
  Serial.print("[CMD from CYD] ");
  Serial.println(command);
  
  if (command == "ping") {
    Serial1.println("pong");
    Serial.println("[PONG] Response sent");
  }
  else if (command.startsWith("{")) {
    // Parse JSON konfigurasi
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, command);
    
    if (err == DeserializationError::Ok) {
      float oldT = THRESH;
      int oldW = DET_WIN;
      int oldM = MIN_VIB;
      
      // Ambil nilai baru dari JSON
      if (doc.containsKey("set")) {
        JsonArray setArray = doc["set"];
        if (setArray.size() >= 3) {
          THRESH = setArray[0] | THRESH;
          DET_WIN = setArray[1] | DET_WIN;
          MIN_VIB = setArray[2] | MIN_VIB;
          cfgRecv = true;
          
          Serial.print("[CFG] Updated: ");
          Serial.print(THRESH); Serial.print(" (was ");
          Serial.print(oldT); Serial.print("), ");
          Serial.print(DET_WIN); Serial.print(" (was ");
          Serial.print(oldW); Serial.print("), ");
          Serial.print(MIN_VIB); Serial.print(" (was ");
          Serial.print(oldM); Serial.println(")");
          
          // Kirim konfirmasi ke CYD
          sendConfigAckToCYD();
          
          // Blink LED 2x untuk indikasi config diterima
          for (int i = 0; i < 2; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
          }
        }
      }
    } else {
      Serial.println("[JSON ERR] Failed to parse config");
      // Kirim error ke CYD
      Serial1.print(CFG_PREFIX);
      Serial1.println("{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    }
  }
}

// ========================================================
// FUNGSI UTILITY
// ========================================================

// Kirim data trip ke CYD
void sendData() {
  unsigned long tripTot = tripAcc;
  if (engOn) {
    tripTot += (millis() - tripStart) / 1000;
  }

  // Buat JSON untuk dikirim ke CYD
  StaticJsonDocument<128> doc;
  doc["eng"] = engOn ? 1 : 0;
  doc["trip"] = tripTot;
  
  String output;
  serializeJson(doc, output);
  
  sendToCYD(output);
  
  // Ringkas output ke serial monitor (debugging)
  unsigned long h = tripTot / 3600;
  unsigned long m = (tripTot % 3600) / 60;
  unsigned long s = tripTot % 60;
  Serial.print("[TX] Engine: ");
  Serial.print(engOn ? "ON" : "OFF");
  Serial.print(" | Trip: ");
  Serial.print(h); Serial.print("h ");
  Serial.print(m); Serial.print("m ");
  Serial.print(s); Serial.println("s");
}

// ========================================================
// SETUP
// ========================================================
void setup() {
  // Serial untuk debugging ke PC
  Serial.begin(115200);
  
  // Serial1 untuk komunikasi dengan CYD (pin 0 = RX, pin 1 = TX)
  Serial1.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Inisialisasi LED indikator (berkedip 3x saat boot)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Inisialisasi sensor ADXL345
  if(!accel.begin()){
    Serial.println("[ERROR] ADXL345 not found!");
    // LED berkedip cepat untuk indikasi error
    while(1){
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN, LOW);
      delay(200);
    }
  }
  
  accel.setRange(ADXL345_RANGE_4_G);
  Serial.println("[SYSTEM] Arduino Pro Micro Ready");
  Serial.println("[SYSTEM] Waiting for CYD connection...");
  
  // Kirim pesan siap ke CYD
  delay(1000);
  StaticJsonDocument<128> initDoc;
  initDoc["status"] = "ready";
  initDoc["threshold"] = THRESH;
  initDoc["detection_window"] = DET_WIN;
  initDoc["min_vibration"] = MIN_VIB;
  
  String initMsg;
  serializeJson(initDoc, initMsg);
  
  Serial1.print(CFG_PREFIX);
  Serial1.println(initMsg);
  Serial1.flush();
  
  Serial.println("[SYSTEM] Initial config sent to CYD");
}

// ========================================================
// LOOP UTAMA
// ========================================================
void loop() {
  unsigned long now = millis();
  
  // ========================================================
  // BACA PERINTAH DARI CYD VIA Serial1
  // ========================================================
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
      if (inputBuffer.length() > 0) {
        // Cek apakah data dari CYD memiliki prefix CMD:
        if (inputBuffer.startsWith(CMD_PREFIX)) {
          // Hapus prefix dan proses command
          String command = inputBuffer.substring(strlen(CMD_PREFIX));
          processCommandFromCYD(command);
        } 
        else if (inputBuffer.startsWith(CFG_PREFIX)) {
          // Konfigurasi langsung (alternatif format)
          String configData = inputBuffer.substring(strlen(CFG_PREFIX));
          processCommandFromCYD(configData);
        }
        else {
          // Data tanpa prefix (mungkin debugging)
          Serial.print("[Serial1 Debug] ");
          Serial.println(inputBuffer);
        }
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }
  
  // ========================================================
  // DETEKSI GETARAN (200ms sampling)
  // ========================================================
  int vibCnt = 0;
  unsigned long startChk = millis();
  sensors_event_t ev;
  bool vibNow = false;
  
  while (millis() - startChk < 200) {
    accel.getEvent(&ev);
    float mag = sqrt(pow(ev.acceleration.x, 2) + 
                     pow(ev.acceleration.y, 2) + 
                     pow(ev.acceleration.z - 9.81, 2));
    
    if (mag > THRESH) {
      vibCnt++;
      vibNow = true;
    }
    
    // Log data sensor setiap 50ms (untuk debugging)
    if (millis() - lastRaw >= 50) {
      Serial.print("X:"); Serial.print(ev.acceleration.x, 1);
      Serial.print(" Y:"); Serial.print(ev.acceleration.y, 1);
      Serial.print(" Z:"); Serial.print(ev.acceleration.z, 1);
      Serial.print(" M:"); Serial.print(mag, 1);
      Serial.print(" C:"); Serial.println(vibCnt);
      lastRaw = millis();
    }
    delay(10);
  }
  
  // ========================================================
  // LOGIKA ENGINE ON/OFF
  // ========================================================
  
  // Update timer getaran
  if (vibNow) {
    lastVib = now;
    if (pendingOff) {
      pendingOff = false;
      Serial.println("[CLR] Pending OFF cancelled");
    }
  }
  
  // Engine ON -> OFF (10 detik tanpa getaran)
  if (engOn && !vibNow && (now - lastVib >= OFF_DELAY)) {
    engOn = false;
    tripAcc += (now - tripStart) / 1000;
    digitalWrite(LED_PIN, LOW);
    Serial.println("[ENGINE] OFF - No vibration detected");
    sendData();  // Kirim data trip ke CYD
    delay(UART_PAUSE);
  }
  
  // Engine OFF -> ON (deteksi minimal 3 getaran dalam 200ms)
  if (!engOn && (vibCnt >= MIN_VIB)) {
    engOn = true;
    tripStart = now;
    lastVib = now;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[ENGINE] ON - Vibration detected");
    sendData();  // Kirim data trip ke CYD
    delay(UART_PAUSE);
  }
  
  // ========================================================
  // STATUS DEBOUNCE (setiap 5 detik)
  // ========================================================
  static unsigned long lastStat = 0;
  if (engOn && (now - lastStat >= 5000)) {
    unsigned long silent = (now - lastVib) / 1000;
    unsigned long tripDuration = tripAcc + ((now - tripStart) / 1000);
    unsigned long h = tripDuration / 3600;
    unsigned long m = (tripDuration % 3600) / 60;
    unsigned long s = tripDuration % 60;
    
    Serial.print("[STATUS] Engine ON | Silent: ");
    Serial.print(silent);
    Serial.print("/10s | Trip: ");
    Serial.print(h); Serial.print("h ");
    Serial.print(m); Serial.print("m ");
    Serial.print(s); Serial.println("s");
    
    lastStat = now;
  }
  
  // ========================================================
  // KIRIM DATA PERIODIK (setiap 60 detik)
  // ========================================================
  if (now - lastSend >= 60000) {
    Serial.println("[PERIODIC] Sending trip data to CYD");
    sendData();
    lastSend = now;
    delay(UART_PAUSE);
  }
  
  // ========================================================
  // BLINK LED SAAT ENGINE ON (efek visual)
  // ========================================================
  if (engOn) {
    static unsigned long lastBlink = 0;
    if (now - lastBlink >= 1000) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = now;
    }
  }
  
  delay(10);  // Loop delay
}