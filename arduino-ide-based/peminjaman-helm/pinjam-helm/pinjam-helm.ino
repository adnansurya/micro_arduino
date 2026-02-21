#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>

// --- KONFIGURASI DEBUG/HARDWARE ---
bool useLCD = false;  // Set ke true jika LCD fisik sudah dipasang

// --- KONFIGURASI WIFI ---
const char* ssid = "MIKRO1";
const char* password = "1DEAlist1";
String scriptID = "AKfycbxHx_dy1aNAhKhkR7ZTckXKQ5_l8uxo38fvIQ5RTP2I1vLMsjhy1vEcItT1pJIHzJN65A";

// --- KONFIGURASI PIN ---
#define SS_PIN 4
#define RST_PIN 5

int laci[] = {25, 26, 27, 14 };
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- FUNGSI SIMULASI LCD KE SERIAL ---
void simulateLCD(String line1, String line2 = "") {
  Serial.println("\n[--- SIMULASI LCD 16x2 ---]");
  Serial.println("| " + line1 + (line1.length() < 16 ? String(std::string(16 - line1.length(), ' ').c_str()) : "") + " |");
  Serial.println("| " + line2 + (line2.length() < 16 ? String(std::string(16 - line2.length(), ' ').c_str()) : "") + " |");
  Serial.println("[-------------------------]\n");
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  if (useLCD) {
    lcd.init();
    lcd.backlight();
  }

  simulateLCD("Hubungkan WiFi..");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  pinMode(laci[0], OUTPUT);
  pinMode(laci[1], OUTPUT);
  pinMode(laci[2], OUTPUT);
  pinMode(laci[3], OUTPUT);

  for (int i = 0; i < 4; i++) {
    digitalWrite(laci[i], LOW);
    delay(500);
    digitalWrite(laci[i], HIGH);
    delay(500);
  }

  simulateLCD("Sistem Ready", "Tap Kartu Anda");
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

  simulateLCD("UID: " + uidString, "Memproses...");

  sendDataToScript(uidString);

  delay(3000);  // Jeda agar user sempat membaca simulasi LCD

  simulateLCD("Sistem Ready", "Tap Kartu Anda");
}

void sendDataToScript(String uid) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);

    HTTPClient http;
    String url = "https://script.google.com/macros/s/" + scriptID + "/exec?uid=" + uid;

    if (http.begin(client, url)) {
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      int httpCode = http.GET();

      if (httpCode > 0) {
        String payload = http.getString();
        displayStatus(payload, uid);
      } else {
        Serial.println("HTTP Error: " + http.errorToString(httpCode));
        simulateLCD("Error Koneksi", "Gagal ke Script");
      }
      http.end();
    }
  }
}


void displayStatus(String res, String uid) {
  // 1. KONDISI: KARTU TIDAK DIKENAL
  if (res == "TIDAK_DIKENAL") {
    if (useLCD) {
      lcd.clear();
      lcd.print("UID:" + uid);
      lcd.setCursor(0, 1);
      lcd.print("TIDAK DIKENALI!");
    }
    simulateLCD("UID:" + uid, "TIDAK DIKENALI!");
    delay(3000);
    return;
  }

  // 2. KONDISI: KARTU DIKENAL (Format: NIM|NAMA|ACTION|LACI)
  // Kita pecah string berdasarkan karakter '|'
  int firstPipe = res.indexOf('|');
  int secondPipe = res.indexOf('|', firstPipe + 1);
  int thirdPipe = res.indexOf('|', secondPipe + 1);

  if (firstPipe != -1 && secondPipe != -1 && thirdPipe != -1) {
    String nim = res.substring(0, firstPipe);
    String nama = res.substring(firstPipe + 1, secondPipe);
    String action = res.substring(secondPipe + 1, thirdPipe);
    String laciStr = res.substring(thirdPipe + 1);
    int nomorLaci = laciStr.toInt();
    int indexLaci = nomorLaci - 1;

    // TAMPILAN TAHAP 1: Nama & NIM (Selama 3 Detik)
    if (useLCD) {
      lcd.clear();
      lcd.print(nama.substring(0, 16));  // Potong jika nama > 16 karakter
      lcd.setCursor(0, 1);
      lcd.print(nim);
    }
    simulateLCD(nama, nim);
    delay(3000);

    // TAMPILAN TAHAP 2: Action & Nomor Laci
    String line1 = (action == "TAP IN") ? "Titip di Laci:" : "Ambil dr Laci:";
    String line2 = "Nomor: " + laciStr;

    if (useLCD) {
      lcd.clear();
      lcd.print(line1);
      lcd.setCursor(0, 1);
      lcd.print(line2);
    }

    simulateLCD(line1, line2);
    digitalWrite(laci[indexLaci], LOW);
    delay(5000);
    digitalWrite(laci[indexLaci], HIGH);
  } else if (res == "PENUH") {
    if (useLCD) {
      lcd.clear();
      lcd.print("Maaf, Laci");
      lcd.setCursor(0, 1);
      lcd.print("Sudah Penuh!");
    }
    simulateLCD("Maaf, Laci", "Sudah Penuh!");
    delay(3000);
  }
}