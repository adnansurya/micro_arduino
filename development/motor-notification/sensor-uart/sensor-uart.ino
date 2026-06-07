#include <Wire.h>
#include <Adafruit_ADXL345_U.h>
#include <ArduinoJson.h>

Adafruit_ADXL345_Unified accel(12345);

// Parameter Getaran
float THRESH = 12.0;
int MIN_VIB = 15;
bool engOn = false;
unsigned long tripStart = 0, tripAcc = 0, lastSend = 0, lastVib = 0;
const int LED_PIN = 17;
const unsigned long OFF_DELAY = 10000;

// TTP225 Touch Sensor (Output LOW saat disentuh)
#define TOUCH_PIN 4
#define LONG_TIME 1000      // Long press detection (ms)
#define DOUBLE_TIME 300     // Double tap detection (ms)

bool touchDetected = false;
unsigned long touchStart = 0, lastTouch = 0;
byte touchCount = 0;
bool waitDouble = false;

// Komunikasi
#define CMD "CMD:"
#define DATA "DATA:"
String inputBuf = "";

void sendToCYD(String s) {
  Serial1.print(DATA);
  Serial1.println(s);
  Serial.println("[TX] " + s);
}

void sendCmd(String act) {
  sendToCYD("{\"t\":\"touch\",\"a\":\"" + act + "\"}");
}

void sendStatus(String st, String msg) {
  sendToCYD("{\"t\":\"stat\",\"s\":\"" + st + "\",\"m\":\"" + msg + "\"}");
}

void sendTrip() {
  unsigned long tot = tripAcc + (engOn ? (millis() - tripStart) / 1000 : 0);
  sendToCYD("{\"eng\":" + String(engOn ? 1 : 0) + ",\"trip\":" + String(tot) + "}");
}

// Baca status touch sensor TTP225
bool isTouched() {
  return digitalRead(TOUCH_PIN) == LOW;  // TTP225 output LOW saat disentuh
}

void procCmd(String c) {
  if (c == "ping") Serial1.println("pong");
  else if (c == "get_config") {
    sendToCYD("{\"th\":" + String(THRESH) + ",\"mv\":" + String(MIN_VIB) + "}");
  }
  else if (c.startsWith("{")) {
    StaticJsonDocument<128> doc;
    if (!deserializeJson(doc, c)) {
      if (doc.containsKey("th")) THRESH = doc["th"];
      if (doc.containsKey("mv")) MIN_VIB = doc["mv"];
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED_PIN, HIGH); delay(100);
        digitalWrite(LED_PIN, LOW); delay(100);
      }
      sendStatus("cfg_ok", "Config saved");
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT_PULLUP);  // Pull-up untuk TTP225
  
  // Test LED
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(100);
    digitalWrite(LED_PIN, LOW); delay(100);
  }
  
  // Inisialisasi ADXL345
  if (!accel.begin()) {
    while (1) {
      digitalWrite(LED_PIN, HIGH); delay(200);
      digitalWrite(LED_PIN, LOW); delay(200);
    }
  }
  accel.setRange(ADXL345_RANGE_4_G);
  
  sendStatus("ready", "TTP225 Touch Sensor Ready");
  Serial.println("[TTP225] Touch sensor initialized on pin " + String(TOUCH_PIN));
}

void loop() {
  unsigned long now = millis();
  
  // ========================================================
  // TTP225 TOUCH HANDLING
  // ========================================================
  bool touched = isTouched();
  
  if (touched && !touchDetected) {
    // Touch mulai
    touchDetected = true;
    touchStart = now;
    Serial.println("[TTP225] Touch detected");
  }
  else if (!touched && touchDetected) {
    // Touch berakhir
    touchDetected = false;
    unsigned long duration = now - touchStart;
    
    if (duration >= LONG_TIME) {
      // LONG PRESS
      sendCmd("long");
      digitalWrite(LED_PIN, HIGH); delay(200); digitalWrite(LED_PIN, LOW);
      touchCount = 0;
      waitDouble = false;
      Serial.println("[TTP225] Long press");
    }
    else if (waitDouble) {
      // DOUBLE TAP
      sendCmd("double");
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED_PIN, HIGH); delay(50);
        digitalWrite(LED_PIN, LOW); delay(50);
      }
      touchCount = 0;
      waitDouble = false;
      Serial.println("[TTP225] Double tap");
    }
    else {
      // Tunggu kemungkinan double tap
      touchCount = 1;
      waitDouble = true;
      lastTouch = now;
      
      // Non-blocking wait for double tap
      unsigned long startWait = millis();
      bool isDouble = false;
      while (millis() - startWait < DOUBLE_TIME) {
        if (isTouched()) {
          isDouble = true;
          while (isTouched()) delay(10);  // Wait for release
          break;
        }
        delay(10);
      }
      
      if (isDouble) {
        waitDouble = false;
        touchCount = 0;
      } else {
        // SINGLE TAP
        sendCmd("single");
        digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW);
        waitDouble = false;
        touchCount = 0;
        Serial.println("[TTP225] Single tap");
      }
    }
  }
  
  // Timeout untuk double tap
  if (waitDouble && (now - lastTouch) > DOUBLE_TIME) {
    waitDouble = false;
    touchCount = 0;
  }
  
  // ========================================================
  // BACA PERINTAH DARI CYD
  // ========================================================
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
      if (inputBuf.length()) {
        if (inputBuf.startsWith(CMD)) procCmd(inputBuf.substring(4));
        inputBuf = "";
      }
    } else inputBuf += c;
  }
  
  // ========================================================
  // DETEKSI GETARAN (200ms sampling)
  // ========================================================
  int vib = 0;
  unsigned long chk = millis();
  sensors_event_t ev;
  bool vibNow = false;
  
  while (millis() - chk < 200) {
    accel.getEvent(&ev);
    float mag = sqrt(pow(ev.acceleration.x, 2) + 
                     pow(ev.acceleration.y, 2) + 
                     pow(ev.acceleration.z - 9.81, 2));
    if (mag > THRESH) { 
      vib++; 
      vibNow = true; 
    }
    delay(5);
  }
  
  // Update timer getaran
  if (vibNow) { 
    lastVib = now; 
  }
  
  // ========================================================
  // LOGIKA ENGINE ON/OFF
  // ========================================================
  
  // Engine ON -> OFF (10 detik tanpa getaran)
  if (engOn && !vibNow && (now - lastVib >= OFF_DELAY)) {
    engOn = false;
    tripAcc += (now - tripStart) / 1000;
    digitalWrite(LED_PIN, LOW);
    sendTrip();
    sendStatus("off", "Engine stopped");
    delay(500);
    Serial.println("[ENGINE] OFF");
  }
  
  // Engine OFF -> ON (deteksi getaran melebihi MIN_VIB)
  if (!engOn && (vib >= MIN_VIB)) {
    engOn = true;
    tripStart = now;
    lastVib = now;
    digitalWrite(LED_PIN, HIGH);
    sendTrip();
    sendStatus("on", "Engine started");
    delay(500);
    Serial.println("[ENGINE] ON");
  }
  
  // ========================================================
  // STATUS PERIODIK (setiap 10 detik saat engine ON)
  // ========================================================
  static unsigned long lastStat = 0;
  if (engOn && (now - lastStat >= 10000)) {
    unsigned long dur = tripAcc + (now - tripStart) / 1000;
    sendStatus("periodic", "Trip: " + String(dur / 3600) + "h " + String((dur % 3600) / 60) + "m");
    lastStat = now;
  }
  
  // ========================================================
  // KIRIM DATA TRIP PERIODIK (setiap 60 detik)
  // ========================================================
  if (now - lastSend >= 60000) {
    sendTrip();
    lastSend = now;
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
  
  delay(10);
}