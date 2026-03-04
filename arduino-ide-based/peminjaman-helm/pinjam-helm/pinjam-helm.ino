#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- LIBRARY WIFIMANAGER ---
#include <WiFiManager.h>

// --- KONFIGURASI SCRIPT ---
String scriptID = "AKfycbxHx_dy1aNAhKhkR7ZTckXKQ5_l8uxo38fvIQ5RTP2I1vLMsjhy1vEcItT1pJIHzJN65A";

// --- KONFIGURASI PIN ---
#define SS_PIN 4
#define RST_PIN 5

int laci[] = { 25, 26, 27, 14 };
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Booting..");

  // --- INISIALISASI WIFIMANAGER ---
  WiFiManager wm;

  lcd.setCursor(0, 1);
  lcd.print("Cek Koneksi...");

  // Jika gagal konek, ESP32 akan membuat AP bernama "Sistem-Loker-Helm"
  // Buka HP, konek ke WiFi tersebut, lalu buka browser ke 192.168.4.1
  if (!wm.autoConnect("Sistem-Loker-Helm")) {
    lcd.clear();
    lcd.print("Gagal Setup");
    delay(3000);
    ESP.restart();
  }

  // Jika sampai sini, berarti sudah terkoneksi
  lcd.clear();
  lcd.print("WiFi Terhubung!");
  delay(2000);

  // Setup Pin Laci
  for (int i = 0; i < 4; i++) {
    pinMode(laci[i], OUTPUT);
    digitalWrite(laci[i], HIGH);
  }

  lcd.clear();
  lcd.print("Sistem Ready");
  lcd.setCursor(0, 1);
  lcd.print("Tap Kartu Anda");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
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

  delay(2000);
  lcd.clear();
  lcd.print("Sistem Ready");
  lcd.setCursor(0, 1);
  lcd.print("Tap Kartu Anda");
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
        String payload = http.getString();
        displayStatus(payload, uid);
      } else {
        lcd.clear();
        lcd.print("Error Server");
      }
      http.end();
    }
  } else {
    lcd.clear();
    lcd.print("WiFi Terputus");
  }
}

void displayStatus(String res, String uid) {
  if (res == "TIDAK_DIKENAL") {
    lcd.clear();
    lcd.print("UID:" + uid);
    lcd.setCursor(0, 1);
    lcd.print("TIDAK DIKENALI!");
    delay(5000);
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

    if (nama == "Mastercard") {
      lcd.clear();
      lcd.print("MASTER ACCESS");
      lcd.setCursor(0, 1);
      lcd.print("OPEN ALL KEYS");
      for (int i = 0; i < 4; i++) {
        digitalWrite(laci[i], LOW);
        delay(5000);
        digitalWrite(laci[i], HIGH);
      }
      return;
    }

    lcd.clear();
    lcd.print(nama.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(nim);
    delay(2000);

    lcd.clear();
    lcd.print((action == "TAP IN") ? "Titip di Laci:" : "Ambil dr Laci:");
    lcd.setCursor(0, 1);
    lcd.print("Nomor: " + laciStr);

    int indexLaci = laciStr.toInt() - 1;
    if (indexLaci >= 0 && indexLaci < 4) {
      digitalWrite(laci[indexLaci], LOW);
      delay(10000);
      digitalWrite(laci[indexLaci], HIGH);
    }
  } else if (res == "PENUH") {
    lcd.clear();
    lcd.print("Maaf, Laci");
    lcd.setCursor(0, 1);
    lcd.print("Sudah Penuh!");
    delay(3000);
  }
}