#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>

// --- KONFIGURASI SCRIPT ---
String scriptID = "AKfycbxHx_dy1aNAhKhkR7ZTckXKQ5_l8uxo38fvIQ5RTP2I1vLMsjhy1vEcItT1pJIHzJN65A";

// --- KONFIGURASI PIN ---
#define SS_PIN 4
#define RST_PIN 5
int laci[] = { 25, 26, 27, 14 };

// --- VARIABEL NON-BLOCKING ---
unsigned long relayOffTime[4] = { 0, 0, 0, 0 };  // Waktu kapan masing-masing laci harus mati
const unsigned long DURASI_BUKA = 10000;         // 10 detik

unsigned long lcdResetTime = 0;  // Kapan LCD harus kembali ke "Ready"

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();
  lcd.print("Sistem Booting..");

  WiFiManager wm;
  if (!wm.autoConnect("Sistem-Loker-Helm")) {
    ESP.restart();
  }

  for (int i = 0; i < 4; i++) {
    pinMode(laci[i], OUTPUT);
    digitalWrite(laci[i], HIGH);  // Relay OFF (Active Low)
  }

  lcd.clear();
  lcd.print("Sistem Ready");
  lcd.setCursor(0, 1);
  lcd.print("Tap Kartu Anda");
}

void loop() {
  unsigned long currentMillis = millis();

  // --- LOGIKA NON-BLOCKING RELAY & LCD RESET ---
  bool anyRelayActive = false;
  for (int i = 0; i < 4; i++) {
    if (relayOffTime[i] > 0) {
      if (currentMillis >= relayOffTime[i]) {
        digitalWrite(laci[i], HIGH);
        relayOffTime[i] = 0;
        Serial.println("Laci " + String(i + 1) + " Terkunci.");
      } else {
        anyRelayActive = true;  // Masih ada laci yang terbuka
      }
    }
  }

  // Jika tidak ada laci aktif dan sudah waktunya reset LCD
  if (!anyRelayActive && lcdResetTime > 0 && currentMillis >= lcdResetTime) {
    lcd.clear();
    lcd.print("Sistem Ready");
    lcd.setCursor(0, 1);
    lcd.print("Tap Kartu Anda");
    lcd.backlight();
    lcdResetTime = 0;  // Reset timer LCD agar tidak clear terus-menerus
  }

  // Scan RFID
  if (!rfid.PICC_IsNewCardPresent()){ !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  lcd.clear();
  lcd.print("Memproses...");

  sendDataToScript(uidString);

  // Berikan jeda sedikit agar LCD tidak langsung balik ke "Ready" sebelum user baca respon
  // Tapi tetap gunakan non-blocking jika ingin benar-benar responsif
}

void sendDataToScript(String uid) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url = "https://script.google.com/macros/s/" + scriptID + "/exec?uid=" + uid;

    if (http.begin(client, url)) {
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      int httpCode = http.GET();
      if (httpCode > 0) {
        displayStatus(http.getString(), uid);
      } else {
        lcd.clear();
        lcd.print("Error Server");
      }
      http.end();
    }
  }
}

void displayStatus(String res, String uid) {
  if (res == "TIDAK_DIKENAL") {
    lcd.clear();
    lcd.print("UID:" + uid);
    lcd.setCursor(0, 1);
    lcd.print("TIDAK DIKENALI!");
    return;
  }

  int firstPipe = res.indexOf('|');
  int secondPipe = res.indexOf('|', firstPipe + 1);
  int thirdPipe = res.indexOf('|', secondPipe + 1);

  if (firstPipe != -1 && secondPipe != -1 && thirdPipe != -1) {
    String nim = res.substring(0, firstPipe);
    String nama = res.substring(firstPipe + 1, secondPipe);
    String action = res.substring(secondPipe + 1, thirdPipe);
    String laciStr = res.substring(thirdPipe + 1);

    // MASTER CARD LOGIC
    if (nama == "Mastercard") {
      lcd.clear();
      lcd.print("MASTER ACCESS");
      lcd.setCursor(0, 1);
      lcd.print("ALL OPEN 10S");

      for (int i = 0; i < 4; i++) {
        digitalWrite(laci[i], LOW);
        relayOffTime[i] = millis() + DURASI_BUKA;
      }
      lcdResetTime = millis() + DURASI_BUKA + 500;  // Reset LCD 0.5 detik setelah relay mati
      return;
    }

    // USER LOGIC
    lcd.clear();
    lcd.print(nama.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(action);  // Tampilkan TAP IN / TAP OUT

    // --- LOGIKA USER BIASA ---
    int indexLaci = laciStr.toInt() - 1;
    if (indexLaci >= 0 && indexLaci < 4) {
      digitalWrite(laci[indexLaci], LOW);
      relayOffTime[indexLaci] = millis() + DURASI_BUKA;
      lcdResetTime = millis() + DURASI_BUKA + 500;

      lcd.clear();
      lcd.print(nama.substring(0, 16));
      lcd.setCursor(0, 1);
      lcd.print("Laci: " + laciStr + " (" + action + ")");
    }
  } else if (res == "PENUH") {
    lcd.clear();
    lcd.print("Maaf, Laci");
    lcd.setCursor(0, 1);
    lcd.print("Sudah Penuh!");
  }
}