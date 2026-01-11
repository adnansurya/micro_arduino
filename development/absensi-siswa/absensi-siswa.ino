#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>  // Library by tzapu
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include "time.h"

// Pin RFID
#define SS_PIN 4
#define RST_PIN 5
MFRC522 mfrc522(SS_PIN, RST_PIN);

LiquidCrystal_I2C lcd(0x27, 16, 2);


// GANTI DENGAN URL APPSCRIPT ANDA
const char* googleUrl = "https://script.google.com/macros/s/AKfycbxl3lLATFYenR28UwWOAt8tJjjQja4gLD9Il_cKTJaddBIjD6z9XNPGEMusqeZimARhLA/exec";

// Konfigurasi NTP (Waktu)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;  // UTC+8 (WITA)
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistem Absensi");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  // WiFiManager
  WiFiManager wm;
  // Membuka portal konfigurasi jika tidak ada WiFi tersimpan
  if (!wm.autoConnect("ESP32_Absensi_AP")) {
    Serial.println("Gagal terhubung, merestart...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("Terhubung ke WiFi!");

  // Ambil Waktu dari Internet
  Serial.print("Retrieving time: ");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Dekatkan kartu RFID ke reader...");
  Serial.println();
  standby();
}

void standby() {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Tempelkan");
  lcd.setCursor(5, 1);
  lcd.print("Kartu");
}

void loop() {
  // Cek jika ada kartu baru
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Mengecek apakah data kartu bisa dibaca
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Ambil UID RFID
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidString += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();
  Serial.println("UID Terdeteksi: " + uidString);

  lcd.clear();
  lcd.print("UID: " + uidString);
  lcd.setCursor(0, 1);
  lcd.print("Mengirim data...");

  // Ambil Waktu Saat Ini
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Gagal mendapatkan waktu");
    return;
  }

  char tanggal[12];
  char jam[10];
  strftime(tanggal, 12, "%Y-%m-%d", &timeinfo);
  strftime(jam, 10, "%H:%M:%S", &timeinfo);

  // Kirim data ke Google Sheets
  if (sendDataToGoogle(uidString, String(tanggal), String(jam))) {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("DATA BERHASIL");
    lcd.setCursor(4, 1);
    lcd.print("DIKIRIM!");
  }else{
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("PENGIRIMAN");
    lcd.setCursor(5, 1);
    lcd.print("GAGAL!");
  }

  delay(3000);

  standby();

  // delay(1000);  // Jeda agar tidak double scan
}

bool sendDataToGoogle(String uid, String tgl, String wkt) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for backup upload");
    return false;
  }

  bool resultOK = false;
  HTTPClient http;
  // Format URL dengan parameter POST
  //String urlFull = String(googleUrl) + "?tag=" + uid + "&date=" + tgl + "&time=" + wkt;

  Serial.println("Mengirim data...");
  http.begin(String(googleUrl).c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(4096);
  doc["date"] = tgl;
  doc["time"] = wkt;
  doc["uid"] = uid;

  String payload;
  serializeJson(doc, payload);

  Serial.println("Payload size: " + String(payload.length()));

  int httpCode = http.POST(payload);

  if (httpCode > -1) {
    Serial.print("Respon: ");
    Serial.println(httpCode);
    resultOK = true;
  } else {
    Serial.print("Error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }
  http.end();
  return resultOK;
}