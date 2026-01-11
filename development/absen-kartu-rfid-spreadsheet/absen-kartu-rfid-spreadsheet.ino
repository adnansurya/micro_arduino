#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- KONFIGURASI WIFI ---
const char* ssid = "MIKRO1";
const char* password = "1DEAlist1";

// --- KONFIGURASI GOOGLE SCRIPT ---
// Masukkan Script ID yang didapat dari Web App URL (kode antara /s/ dan /exec)
String scriptID = "AKfycbxsQUV9MBpK2i4WV0Yzdui-9ggYwDwVjH6iWxJQgV8x32PNB38VeGP-0dhrSM-FssOdNw";

// --- KONFIGURASI PIN RFID & LCD ---
#define SS_PIN 4
#define RST_PIN 5  // Ubah jika perlu ke pin lain
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hubungkan WiFi..");

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  lcd.clear();
  lcd.print("Sistem Ready");
  lcd.setCursor(0, 1);
  lcd.print("Silahkan Tap!");
  Serial.println("\nWiFi Terhubung!");
}

void loop() {
  // Cek apakah ada kartu baru
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Mengecek apakah data kartu bisa dibaca
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Ambil UID Kartu
  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  Serial.println("UID Kartu: " + uidString);
  kirimData(uidString);

  // Berhenti membaca kartu yang sama
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void kirimData(String id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Konstruksi URL
    String url = "https://script.google.com/macros/s/" + scriptID + "/exec?id=" + id;

    lcd.clear();
    lcd.print("Sedang Proses...");

    // Mulai Koneksi
    http.begin(url.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = http.GET();
    String response = http.getString();

    if (httpCode > 0) {
      Serial.println("Respon: " + response);

      // Logika Parsing Respon: "Berhasil:Nama,NomorInduk"
      if (response.indexOf("Berhasil:") >= 0) {
        String data = response.substring(9);  // Ambil setelah kata "Berhasil:"
        int komaIndex = data.indexOf(",");
        String nama = data.substring(0, komaIndex);
        String ni = data.substring(komaIndex + 1);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(nama);
        lcd.setCursor(0, 1);
        lcd.print(ni);
      } else {
        lcd.clear();
        lcd.print("Gagal:");
        lcd.setCursor(0, 1);
        lcd.print("Tak Terdaftar");
      }
    } else {
      Serial.println("Error HTTP: " + String(httpCode));
      lcd.clear();
      lcd.print("Error Koneksi");
    }

    http.end();
    delay(3000);  // Tampilkan pesan selama 3 detik sebelum kembali standby
    lcd.clear();
    lcd.print("Silahkan Tap!");
  }
}