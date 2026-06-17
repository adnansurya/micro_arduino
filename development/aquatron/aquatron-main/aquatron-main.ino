#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h> // Library WiFiManager tambahan

// Helper code untuk Firebase data logging dan token generation
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// 1. Konfigurasi Firebase
#define FIREBASE_HOST "https://aquatron-app-default-rtdb.asia-southeast1.firebasedatabase.app/" 
#define FIREBASE_AUTH "PacMY6HzqqijY7X44xesrqZG8cd1sCsuQNs47JgG"

// 2. Konfigurasi Sensor Suhu (DS18B20) - Paralel di satu pin
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// 3. Konfigurasi Sensor Jarak (HC-SR04)
#define TRIG_PIN_1 12
#define ECHO_PIN_1 14
#define TRIG_PIN_2 27
#define ECHO_PIN_2 26

// 4. Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Data Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variabel untuk pengiriman data berkala
unsigned long sendDataPrevMillis = 0;
const long updateInterval = 5000; // Update ke Firebase setiap 5 detik

// Fungsi untuk membaca jarak sensor ultrasonik
float bacaJarak(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000); // Timeout 30ms jika tidak ada benda
  float distance = duration * 0.034 / 2;
  
  if (distance == 0) return -1; // Indikasi jika sensor out of range / error
  return distance;
}

void setup() {
  Serial.begin(115200); // Menaikkan baudrate ke standar ESP32

  // Inisialisasi Pin Sensor Jarak
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);

  // Inisialisasi Sensor Suhu & LCD
  sensors.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Memulai WiFi...");

  // --- KONFIGURASI WIFI MANAGER ---
  WiFiManager wm;
  
  // Membuat Access Point bernama "ESP32_Aquatron_AP" tanpa password jika tidak ada wifi tersimpan
  lcd.setCursor(0, 1);
  lcd.print("Cek AP: ESP32...");
  
  if (!wm.autoConnect("ESP32_Aquatron_AP")) {
    Serial.println("Gagal terhubung ke WiFi dan waktu habis (timeout)");
    lcd.clear();
    lcd.print("WiFi Gagal!");
    delay(3000);
    ESP.restart(); // Restart jika gagal
  }

  Serial.println("\nTersambung ke Wi-Fi!");
  lcd.clear();
  lcd.print("WiFi Terhubung!");
  delay(1500);
  
  // Inisialisasi Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  lcd.clear();
  lcd.print("Aquatron MultiSns");
  delay(2000);
  lcd.clear();
}

void loop() {
  // 1. Ambil data suhu dari kedua sensor (index 0 dan index 1)
  sensors.requestTemperatures();
  float suhu1 = sensors.getTempCByIndex(0);
  float suhu2 = sensors.getTempCByIndex(1);

  // 2. Ambil data jarak dari kedua sensor ultrasonik
  float jarak1 = bacaJarak(TRIG_PIN_1, ECHO_PIN_1);
  float jarak2 = bacaJarak(TRIG_PIN_2, ECHO_PIN_2);

  // 3. Tampilkan data ke Serial Monitor untuk monitoring cepat
  Serial.print("Suhu1: "); Serial.print(suhu1); Serial.print("C | ");
  Serial.print("Suhu2: "); Serial.print(suhu2); Serial.println("C");
  Serial.print("Jarak1: "); Serial.print(jarak1); Serial.print("cm | ");
  Serial.print("Jarak2: "); Serial.print(jarak2); Serial.println("cm");
  Serial.println("-------------------------------------------------");

  // 4. Tampilkan data bergantian ke LCD 16x2 agar muat
  // Menampilkan Suhu
  lcd.setCursor(0, 0);
  lcd.print("T1:"); lcd.print(suhu1, 1); lcd.print((char)223); lcd.print("C ");
  lcd.setCursor(9, 0);
  lcd.print("T2:"); lcd.print(suhu2, 1); lcd.print((char)223); lcd.print("C ");

  // Menampilkan Jarak
  lcd.setCursor(0, 1);
  lcd.print("J1:"); lcd.print(jarak1, 0); lcd.print("cm   ");
  lcd.setCursor(9, 1);
  lcd.print("J2:"); lcd.print(jarak2, 0); lcd.print("cm   ");

  // 5. Kirim data ke Firebase secara berkala (Non-blocking delay)
  if (millis() - sendDataPrevMillis > updateInterval || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();

    String basePath = "test/sensorData/aquatron_001/latest/";
    
    Serial.println("Mengirim data terstruktur ke Firebase...");
    
    // --- KELOMPOK MAIN (Sensor 1) ---
    // Kirim Suhu 1 ke mainTemperature
    if (suhu1 != DEVICE_DISCONNECTED_C) {
      Firebase.RTDB.setFloat(&fbdo, basePath + "mainTemperature", suhu1);
    }
    // Kirim Jarak 1 ke mainWaterLevel
    if (jarak1 != -1) {
      Firebase.RTDB.setFloat(&fbdo, basePath + "mainWaterLevel", jarak1);
    }

    // --- KELOMPOK RESERVOIR (Sensor 2) ---
    // Kirim Suhu 2 ke reservoirTemperature
    if (suhu2 != DEVICE_DISCONNECTED_C) {
      Firebase.RTDB.setFloat(&fbdo, basePath + "reservoirTemperature", suhu2);
    }
    // Kirim Jarak 2 ke reservoirWaterLevel
    if (jarak2 != -1) {
      Firebase.RTDB.setFloat(&fbdo, basePath + "reservoirWaterLevel", jarak2);
    }
    
    // Opsional: Memperbarui Timestamp waktu pengiriman (Milidetik Epoch Unix)
    // Anda bisa mengaktifkan ini jika ingin memperbarui kolom "updatedAt" secara otomatis
    Firebase.RTDB.setInt(&fbdo, basePath + "updatedAt", millis()); 
  }

  delay(200); // Stabilisasi loop pendek
}