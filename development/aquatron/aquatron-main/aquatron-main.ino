#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>

// Helper code untuk Firebase data logging dan token generation
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// 1. Konfigurasi Firebase
#define FIREBASE_HOST "https://aquatron-app-default-rtdb.asia-southeast1.firebasedatabase.app/" 
#define FIREBASE_AUTH "PacMY6HzqqijY7X44xesrqZG8cd1sCsuQNs47JgG"

// 2. Konfigurasi Sensor Suhu (DS18B20)
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

// Manajemen Waktu (Non-blocking)
unsigned long sendDataPrevMillis = 0;
const long updateInterval = 5000;    // Kirim data setiap 5 detik

unsigned long changePagePrevMillis = 0;
const long pageInterval = 4000;      // Ganti halaman LCD setiap 4 detik

// Variabel Kontrol Tampilan LCD
int currentPage = 0;                 // 0:Suhu, 1:Water Level, 2:pH, 3:Pompa
bool isSending = false;              // Status indikator panah pengiriman data
unsigned long arrowTurnOffMillis = 0;

// Byte kustom untuk karakter panah atas (Custom Character)
byte panahAtas[8] = {
  B00100,
  B01110,
  B11111,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100
};

// Variabel Sensor & Dummy
float suhu1 = 0, suhu2 = 0;
float jarak1 = 0, jarak2 = 0;
float dummyPH = 7.35;               // Nilai dummy sensor pH
String dummyPompa1 = "ON";          // Status dummy pompa 1
String dummyPompa2 = "OFF";         // Status dummy pompa 2

// Fungsi membaca jarak ultrasonik
float bacaJarak(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000);
  float distance = duration * 0.034 / 2;
  
  if (distance == 0) return -1;
  return distance;
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);

  sensors.begin();
  
  // Inisialisasi LCD & Karakter Kustom
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, panahAtas); // Daftarkan karakter panah sebagai karakter index ke-0
  
  lcd.setCursor(0, 0);
  lcd.print("Memulai WiFi...");

  // WiFiManager
  WiFiManager wm;
  lcd.setCursor(0, 1);
  lcd.print("Cek AP: ESP32...");
  
  if (!wm.autoConnect("ESP32_Aquatron_AP")) {
    Serial.println("Gagal konek WiFi, merestart...");
    ESP.restart();
  }

  Serial.println("\nTersambung ke Wi-Fi!");
  lcd.clear();
  lcd.print("WiFi Terhubung!");
  delay(1500);
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  lcd.clear();
}

void loop() {
  // ==========================================
  // 1. MEMBACA DATA SENSOR
  // ==========================================
  sensors.requestTemperatures();
  suhu1 = sensors.getTempCByIndex(0);
  suhu2 = sensors.getTempCByIndex(1);
  jarak1 = bacaJarak(TRIG_PIN_1, ECHO_PIN_1);
  jarak2 = bacaJarak(TRIG_PIN_2, ECHO_PIN_2);

  // ==========================================
  // 2. LOGIKA PERGANTIAN HALAMAN LCD (4 DETIK)
  // ==========================================
  bool perluUpdateLayar = false;

  if (millis() - changePagePrevMillis > pageInterval) {
    changePagePrevMillis = millis();
    currentPage++;
    if (currentPage > 3) currentPage = 0;
    
    lcd.clear(); 
    perluUpdateLayar = true; 
  }

  // Menampilkan data ke LCD jika halaman berganti
  if (perluUpdateLayar) {
    switch (currentPage) {
      case 0:
        lcd.setCursor(0, 0); lcd.print("TEMP");
        lcd.setCursor(0, 1); lcd.print("M:"); lcd.print(suhu1, 1); lcd.print((char)223); lcd.print("C ");
        lcd.setCursor(9, 1); lcd.print("R:"); lcd.print(suhu2, 1); lcd.print((char)223); lcd.print("C");
        break;

      case 1:
        lcd.setCursor(0, 0); lcd.print("W. LEVEL");
        lcd.setCursor(0, 1); lcd.print("M:"); lcd.print(jarak1, 0); lcd.print("cm ");
        lcd.setCursor(9, 1); lcd.print("R:"); lcd.print(jarak2, 0); lcd.print("cm");
        break;

      case 2:
        lcd.setCursor(0, 0); lcd.print("PH METER");
        lcd.setCursor(0, 1); lcd.print("Nilai pH : "); lcd.print(dummyPH, 2);
        break;

      case 3:
        lcd.setCursor(0, 0); lcd.print("PUMP ST.");
        lcd.setCursor(0, 1); lcd.print("P1:"); lcd.print(dummyPompa1);
        lcd.setCursor(9, 1); lcd.print("P2:"); lcd.print(dummyPompa2);
        break;
    }
    
    if (isSending) {
      lcd.setCursor(15, 0);
      lcd.write(0);
    }
  }

  // ==========================================
  // 3. LOGIKA PENGIRIMAN DATA JSON (1x TEMBAK)
  // ==========================================
  if (millis() - sendDataPrevMillis > updateInterval || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();
    isSending = true; 
    arrowTurnOffMillis = millis() + 1000; // Indikator panah menyala 1 detik

    // Munculkan indikator panah secara instan di LCD
    lcd.setCursor(15, 0);
    lcd.write(0);

    String path = "test/sensorData/aquatron_001/latest";
    Serial.println("Menggabungkan data ke JSON & Mengirim...");

    // Membuat objek JSON bawaan dari library Firebase ESP Client
    FirebaseJson json;
    
    // Memasukkan data ke JSON hanya jika sensor valid/terdeteksi
    if (suhu1 != DEVICE_DISCONNECTED_C) {
      json.set("mainTemperature", suhu1);
    }
    if (jarak1 != -1) {
      json.set("mainWaterLevel", jarak1);
    }
    if (suhu2 != DEVICE_DISCONNECTED_C) {
      json.set("reservoirTemperature", suhu2);
    }
    if (jarak2 != -1) {
      json.set("reservoirWaterLevel", jarak2);
    }
    
    // Memasukkan data dummy ke dalam JSON
    json.set("ph", dummyPH);

    // Mengirimkan semua data sekaligus menggunakan metode updateNode
    // updateNode akan memperbarui data di dalam folder 'latest' tanpa menghapus data lain yang sudah ada di sana (seperti ammoniaRisk)
    if (Firebase.RTDB.updateNode(&fbdo, path, &json)) {
      Serial.println("Kirim JSON BERHASIL!");
    } else {
      Serial.print("Kirim JSON GAGAL! Alasan: ");
      Serial.println(fbdo.errorReason());
    }
  }

  // Menghentikan status indikator panah secara independen
  if (isSending && millis() > arrowTurnOffMillis) {
    isSending = false;
    lcd.setCursor(15, 0);
    lcd.print(" "); 
  }

  delay(50); 
}