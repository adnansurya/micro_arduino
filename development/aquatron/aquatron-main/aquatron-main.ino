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

// 4. Konfigurasi Pin Relay untuk Pompa
#define RELAY_PUMP_1 4  // Jalur ke Relay Pompa Main
#define RELAY_PUMP_2 5  // Jalur ke Relay Pompa Reservoir

// 5. Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Data Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Manajemen Waktu Non-blocking
unsigned long firebasePrevMillis = 0;
const long firebaseInterval = 5000;
bool toggleTask = true;
bool firebaseReadyToTrigger = true;

unsigned long changePagePrevMillis = 0;
const long pageInterval = 2000;

// Variabel Kontrol Tampilan LCD
int currentPage = 0;
int iconStatus = 0;
unsigned long iconTurnOffMillis = 0;

// Byte kustom untuk karakter panah atas & bawah
byte panahAtas[8] = {
  B00100, B01110, B11111, B00100, B00100, B00100, B00100, B00100
};

byte panahBawah[8] = {
  B00100, B00100, B00100, B00100, B00100, B11111, B01110, B00100
};

// Variabel Sensor & Dummy
float suhu1 = 0, suhu2 = 0;
float jarak1 = 0, jarak2 = 0;
float dummyPH = 7.00;
String dummyPompa1 = "OFF";
String dummyPompa2 = "OFF";

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
  randomSeed(analogRead(34));

  // Inisialisasi Pin Relay
  pinMode(RELAY_PUMP_1, OUTPUT);
  pinMode(RELAY_PUMP_2, OUTPUT);

  // Kondisi awal: Matikan pompa saat ESP32 pertama kali menyala
  // Catatan: Jika menggunakan modul relay Active-LOW, ubah HIGH menjadi LOW atau sebaliknya
  digitalWrite(RELAY_PUMP_1, LOW);
  digitalWrite(RELAY_PUMP_2, LOW);

  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);

  sensors.begin();

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, panahAtas);
  lcd.createChar(1, panahBawah);

  lcd.setCursor(0, 0);
  lcd.print("Memulai WiFi...");

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

  // Konfigurasi Kredensial Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  // === SET TIMEOUT FIREBASE MENJADI 5 DETIK ===
  config.timeout.serverResponse = 5000;  // Mengatur timeout respon server menjadi 5 detik

  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  lcd.clear();
  firebasePrevMillis = millis() - firebaseInterval;
}

void loop() {
  // ==========================================
  // 1. MEMBACA DATA SENSOR INTERNAL ESP32
  // ==========================================
  sensors.requestTemperatures();
  suhu1 = sensors.getTempCByIndex(0);
  suhu2 = sensors.getTempCByIndex(1);
  jarak1 = bacaJarak(TRIG_PIN_1, ECHO_PIN_1);
  jarak2 = bacaJarak(TRIG_PIN_2, ECHO_PIN_2);

  // ==========================================
  // 2. LOGIKA PERGANTIAN HALAMAN LCD (2 DETIK)
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
        lcd.setCursor(0, 0);
        lcd.print("TEMP");
        lcd.setCursor(0, 1);
        lcd.print("M:");
        lcd.print(suhu1, 1);
        lcd.print((char)223);
        lcd.print("C ");
        lcd.setCursor(9, 1);
        lcd.print("R:");
        lcd.print(suhu2, 1);
        lcd.print((char)223);
        lcd.print("C");
        break;

      case 1:
        lcd.setCursor(0, 0);
        lcd.print("W. LEVEL");
        lcd.setCursor(0, 1);
        lcd.print("M:");
        lcd.print(jarak1, 0);
        lcd.print("cm ");
        lcd.setCursor(9, 1);
        lcd.print("R:");
        lcd.print(jarak2, 0);
        lcd.print("cm");
        break;

      case 2:
        lcd.setCursor(0, 0);
        lcd.print("PH METER");
        lcd.setCursor(0, 1);
        lcd.print("Nilai pH : ");
        lcd.print(dummyPH, 2);
        break;

      case 3:
        lcd.setCursor(0, 0);
        lcd.print("PUMP ST.");
        lcd.setCursor(0, 1);
        lcd.print("P1:");
        lcd.print(dummyPompa1);
        lcd.setCursor(9, 1);
        lcd.print("P2:");
        lcd.print(dummyPompa2);
        break;
    }

    if (iconStatus == 1) {
      lcd.setCursor(15, 0);
      lcd.write(0);
    } else if (iconStatus == 2) {
      lcd.setCursor(15, 0);
      lcd.write(1);
    }
  }

  // ==========================================
  // 3. LOGIKA AKTIVITAS FIREBASE (SINKRONISASI & RELAY)
  // ==========================================
  if (!firebaseReadyToTrigger && (millis() - firebasePrevMillis >= firebaseInterval)) {
    firebaseReadyToTrigger = true;
  }

  if (firebaseReadyToTrigger) {
    firebaseReadyToTrigger = false;
    iconTurnOffMillis = millis() + 1000;

    if (toggleTask) {
      // -----------------------------------------------------------------
      // TUGAS A: KIRIM DATA SENSOR (Indikator Panah Atas)
      // -----------------------------------------------------------------
      iconStatus = 1;
      lcd.setCursor(15, 0);
      lcd.write(0);

      dummyPH = random(200, 1201) / 100.0;

      float selisihSuhu = abs(suhu1 - suhu2);
      bool phAbnormal = (dummyPH < 6.0 || dummyPH > 7.5);
      bool suhuStabil = (suhu1 != DEVICE_DISCONNECTED_C && suhu2 != DEVICE_DISCONNECTED_C && selisihSuhu <= 0.5);

      if (phAbnormal && suhuStabil) {
        Serial.println("[KIRIM] DARURAT! Mengaktifkan kedua pompa via Firebase...");
        FirebaseJson jsonPumps;
        jsonPumps.set("mainPump", 1);
        jsonPumps.set("reservoirPump", 1);
        Firebase.RTDB.updateNode(&fbdo, "test/pumps", &jsonPumps);

        // Langsung nyalakan fisik relay demi keamanan tanpa menunggu giliran TUGAS B
        digitalWrite(RELAY_PUMP_1, HIGH);
        digitalWrite(RELAY_PUMP_2, HIGH);
        dummyPompa1 = "ON";
        dummyPompa2 = "ON";
      }

      String path = "test/sensorData/aquatron_001/latest";
      FirebaseJson json;
      if (suhu1 != DEVICE_DISCONNECTED_C) json.set("mainTemperature", suhu1);
      if (jarak1 != -1) json.set("mainWaterLevel", jarak1);
      if (suhu2 != DEVICE_DISCONNECTED_C) json.set("reservoirTemperature", suhu2);
      if (jarak2 != -1) json.set("reservoirWaterLevel", jarak2);
      json.set("ph", dummyPH);

      if (Firebase.RTDB.updateNode(&fbdo, path, &json)) {
        Serial.println("[KIRIM] Berhasil memperbarui data sensor.");
      } else {
        Serial.print("[KIRIM] Gagal/Timeout: ");
        Serial.println(fbdo.errorReason());
      }

    } else {
      // -----------------------------------------------------------------
      // TUGAS B: BACA STATUS POMPA & KONTROL FISIK RELAY (Indikator Panah Bawah)
      // -----------------------------------------------------------------
      iconStatus = 2;
      lcd.setCursor(15, 0);
      lcd.write(1);

      Serial.println("[BACA] Mengambil status pompa dari test/pumps...");

      if (Firebase.RTDB.getJSON(&fbdo, "test/pumps")) {
        FirebaseJson &jsonResult = fbdo.jsonObject();
        FirebaseJsonData jsonData;

        // Kontrol fisik Relay 1 (Main Pump)
        jsonResult.get(jsonData, "mainPump");
        if (jsonData.success) {
          int p1 = jsonData.to<int>();
          digitalWrite(RELAY_PUMP_1, p1);  // Logika High/Low mengikuti nilai 1/0 dari Firebase
          dummyPompa1 = (p1 == 1) ? "ON" : "OFF";
        }

        // Kontrol fisik Relay 2 (Reservoir Pump)
        jsonResult.get(jsonData, "reservoirPump");
        if (jsonData.success) {
          int p2 = jsonData.to<int>();
          digitalWrite(RELAY_PUMP_2, p2);  // Logika High/Low mengikuti nilai 1/0 dari Firebase
          dummyPompa2 = (p2 == 1) ? "ON" : "OFF";
        }

        Serial.println("[BACA] Berhasil menyelaraskan fisik relay.");
      } else {
        Serial.print("[BACA] Gagal/Timeout mengambil status pompa: ");
        Serial.println(fbdo.errorReason());
      }
    }

    firebasePrevMillis = millis();
    toggleTask = !toggleTask;
  }

  // Menghapus ikon indikator panah setelah 1 detik
  if (iconStatus != 0 && millis() > iconTurnOffMillis) {
    iconStatus = 0;
    lcd.setCursor(15, 0);
    lcd.print(" ");
  }

  delay(50);
}