#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
// #include <LiquidCrystal_I2C.h>

// --- KONFIGURASI WIFI ---
const char* ssid = "MIKRO1";  // SSID default Wokwi
const char* password = "1DEAlist1";  
// Masukkan Deployment ID dari Apps Script Anda
String scriptID = "AKfycbyVV633Z-j-Aqa1PuVEbegz_lXmaU2C581cWtD3UCEr1_GCitDXNQ7SHzEB6T7PRQvbKw"; 

// --- KONFIGURASI PIN ---
#define SS_PIN  4
#define RST_PIN 5
MFRC522 rfid(SS_PIN, RST_PIN);
// LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  
  // lcd.init();
  // lcd.backlight();
  // lcd.print("Hubungkan WiFi..");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
//   lcd.clear();
//   lcd.print("Sistem Ready");
//   lcd.setCursor(0, 1);
//   lcd.print("Tap Kartu Anda");
}


void loop() {
  // Cek jika ada kartu baru
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Ambil UID Kartu
  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uidString += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  Serial.println("UID Terdeteksi: " + uidString);
  // lcd.clear();
  // lcd.print("UID:");
  // lcd.print(uidString);
  // lcd.setCursor(0,1);
  // lcd.print("Memproses...");

  sendDataToScript(uidString);

  // Delay agar tidak terbaca berulang kali
  delay(3000); 
  // lcd.clear();
  // lcd.print("Sistem Ready");
  // lcd.setCursor(0, 1);
  // lcd.print("Tap Kartu Anda");
}

void sendDataToScript(String uid) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Format URL untuk GET request
    String url = "https://script.google.com/macros/s/" + scriptID + "/exec?uid=" + uid;
    
    http.begin(url.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http.GET();
    String payload = http.getString();

    if (httpCode > 0) {
      Serial.println("Respon: " + payload);
      displayStatus(payload);
    } else {
      Serial.println("ERROR KONEKSI");
      // lcd.clear();
      // lcd.print("Error Koneksi");
    }
    http.end();
  }
}

void displayStatus(String res) {
  // lcd.clear();
  if (res == "TIDAK_DIKENAL") {
    // lcd.print("Kartu Tidak");
    // lcd.setCursor(0, 1);
    // lcd.print("Terdaftar!");
  } 
  else if (res.startsWith("IN:")) {
    String laci = res.substring(3);
    // lcd.print("Silakan Titip");
    // lcd.setCursor(0, 1);
    // lcd.print("Laci: " + laci);
  } 
  else if (res.startsWith("OUT:")) {
    String laci = res.substring(4);
    // lcd.print("Silakan Ambil");
    // lcd.setCursor(0, 1);
    // lcd.print("Laci: " + laci);
  } 
  else if (res == "PENUH") {
    // lcd.print("Maaf, Laci");
    // lcd.setCursor(0, 1);
    // lcd.print("Sudah Penuh!");
  }
}