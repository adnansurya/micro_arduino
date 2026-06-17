#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>

// Sediakan helper code untuk Firebase data logging dan token generation
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// 1. Konfigurasi Wi-Fi
#define WIFI_SSID "YUSUF MAULANA"
#define WIFI_PASSWORD "isma2006"

// 2. Konfigurasi Firebase
#define FIREBASE_HOST "https://aquatron-app-default-rtdb.asia-southeast1.firebasedatabase.app/" 
#define FIREBASE_AUTH "PacMY6HzqqijY7X44xesrqZG8cd1sCsuQNs47JgG"

// 3. Konfigurasi Sensor & Perangkat
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Data Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variabel untuk pengiriman data berkala
unsigned long sendDataPrevMillis = 0;
const long updateInterval = 5000; // Update ke Firebase setiap 5 detik

void setup() {
  Serial.begin(9600);

  // Inisialisasi Sensor & LCD
  sensors.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Menghubungkan...");

  // Koneksi ke Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan ke Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nTersambung ke Wi-Fi!");
  
  // Inisialisasi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DS18B20 + Cloud");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Ambil data suhu dari sensor
  sensors.requestTemperatures();
  float suhu = sensors.getTempCByIndex(0);

  // Cek validitas suhu sebelum menampilkan/mengirim
  if (suhu == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: Sensor tidak terdeteksi!");
    lcd.setCursor(0, 0);
    lcd.print("Error Sensor    ");
    return;
  }

  // Tampilkan di Serial Monitor
  Serial.print("Suhu Saat Ini = ");
  Serial.print(suhu);
  Serial.println(" C");

  // Tampilkan di LCD
  lcd.setCursor(0, 0);
  lcd.print("Suhu:");
  lcd.setCursor(0, 1);
  lcd.print(suhu);
  lcd.print((char)223); // Simbol derajat
  lcd.print("C   ");

  // Kirim data ke Firebase secara berkala (Non-blocking delay)
  if (millis() - sendDataPrevMillis > updateInterval || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();

    // Jalur direktori spesifik sesuai database kamu
    String path = "test/sensorData/aquatron_001/latest/mainTemperature";

    Serial.print("Mengirim ke Firebase... ");
    
    // Menggunakan fungsi setFloat karena tipe data 'suhu' adalah float
    if (Firebase.RTDB.setFloat(&fbdo, path, suhu)) {
      Serial.println("BERHASIL!");
    } else {
      Serial.print("GAGAL! Alasan: ");
      Serial.println(fbdo.errorReason());
    }
  }

  delay(100); 
}}