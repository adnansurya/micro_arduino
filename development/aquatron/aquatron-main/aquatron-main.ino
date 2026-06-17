#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>
#include <time.h> 

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
#define RELAY_PUMP_1 4   
#define RELAY_PUMP_2 5   

// 5. Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Data Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Konfigurasi NTP Server (Disesuaikan ke WITA UTC+8)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800;     // UTC+8 untuk WITA (Makassar, Bali, dll.)
const int   daylightOffset_sec = 0;

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

// Fungsi mengambil Timestamp Epoch terkini
unsigned long getEpochTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0; 
  }
  time(&now);
  return now;
}

// Fungsi untuk mencetak Tanggal & Waktu Lokal terformat (WITA)
void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Gagal mendapatkan waktu lokal");
    return;
  }
  // Mencetak dengan format: Hari, Tanggal/Bulan/Tahun Jam:Menit:Detik
  Serial.print("Waktu Sistem Saat Ini (WITA): ");
  Serial.println(&timeinfo, "%A, %d/%m/%Y %H:%M:%S");
}

// Fungsi cetak nilai variabel ke Serial Monitor untuk Debugging
void printDebugData() {
  Serial.println("\n=== [DEBUG] DATA VARIABEL TERKINI ===");
  printLocalTime(); // Menampilkan waktu lokal terformat di dalam log debug
  
  Serial.print("Main Temperature (Suhu 1)      : "); 
  if (suhu1 == DEVICE_DISCONNECTED_C) Serial.println("TERPUTUS (-127C)"); else { Serial.print(suhu1, 2); Serial.println(" C"); }
  
  Serial.print("Reservoir Temperature (Suhu 2) : "); 
  if (suhu2 == DEVICE_DISCONNECTED_C) Serial.println("TERPUTUS (-127C)"); else { Serial.print(suhu2, 2); Serial.println(" C"); }
  
  Serial.print("Main Water Level (Jarak 1)     : "); 
  if (jarak1 == -1) Serial.println("ERROR / NO ECHO"); else { Serial.print(jarak1, 2); Serial.println(" cm"); }
  
  Serial.print("Reservoir Water Level (Jarak 2): "); 
  if (jarak2 == -1) Serial.println("ERROR / NO ECHO"); else { Serial.print(jarak2, 2); Serial.println(" cm"); }
  
  Serial.print("Dummy pH Value (Range 5-8)     : "); Serial.println(dummyPH, 2);
  Serial.print("Status Relay / Tampilan P1     : "); Serial.println(dummyPompa1);
  Serial.print("Status Relay / Tampilan P2     : "); Serial.println(dummyPompa2);
  Serial.print("Current Epoch Time (NTP)       : "); Serial.println(getEpochTime());
  Serial.println("======================================");
}

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

  pinMode(RELAY_PUMP_1, OUTPUT);
  pinMode(RELAY_PUMP_2, OUTPUT);
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
  
  // Sinkronisasi Waktu via NTP Server (WITA)
  lcd.setCursor(0, 1);
  lcd.print("Sinkron WITA...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Validasi tunggu sampai NTP berhasil sinkron (maksimal 10 kali coba)
  struct tm timeinfo;
  int retry = 0;
  while(!getLocalTime(&timeinfo) && retry < 10) {
    Serial.print(".");
    delay(500);
    retry++;
  }
  Serial.println("");
  
  // === MENAMPILKAN TANGGAL & WAKTU SEBELUM MASUK LOOP ===
  Serial.println("\n==========================================");
  Serial.println(" KONEKSI BERHASIL & SISTEM SIAP ");
  printLocalTime(); 
  Serial.println("==========================================\n");
  delay(1500);
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  config.timeout.serverResponse = 5000;  
  
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
    
    if (iconStatus == 1) { lcd.setCursor(15, 0); lcd.write(0); }
    else if (iconStatus == 2) { lcd.setCursor(15, 0); lcd.write(1); }
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

    // Acak nilai pH di kisaran 5.0 - 8.0 sebelum debug diprint
    dummyPH = random(500, 801) / 100.0; 

    // Tampilkan data ke serial monitor tepat sebelum proses komunikasi Firebase dimulai
    printDebugData();

    if (toggleTask) {
      // -----------------------------------------------------------------
      // TUGAS A: KIRIM DATA SENSOR + TIMESTAMP NTP
      // -----------------------------------------------------------------
      iconStatus = 1;
      lcd.setCursor(15, 0);
      lcd.write(0); 

      float selisihSuhu = abs(suhu1 - suhu2);
      bool phAbnormal = (dummyPH < 6.0 || dummyPH > 7.5);
      bool suhuStabil = (suhu1 != DEVICE_DISCONNECTED_C && suhu2 != DEVICE_DISCONNECTED_C && selisihSuhu <= 0.5);

      if (phAbnormal && suhuStabil) {
        Serial.println("[KIRIM] DARURAT! Mengaktifkan kedua pompa...");
        FirebaseJson jsonPumps;
        jsonPumps.set("mainPump", 1);
        jsonPumps.set("reservoirPump", 1);
        Firebase.RTDB.updateNode(&fbdo, "test/pumps", &jsonPumps);
        
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
      
      unsigned long currentEpoch = getEpochTime();
      if (currentEpoch != 0) {
        json.set("updatedAt", currentEpoch);
      }

      if (Firebase.RTDB.updateNode(&fbdo, path, &json)) {
        Serial.println("[KIRIM] Berhasil memperbarui data sensor + updatedAt.");
      } else {
        Serial.print("[KIRIM] Gagal: "); Serial.println(fbdo.errorReason());
      }

    } else {
      // -----------------------------------------------------------------
      // TUGAS B: BACA STATUS POMPA & KONTROL FISIK RELAY
      // -----------------------------------------------------------------
      iconStatus = 2;
      lcd.setCursor(15, 0);
      lcd.write(1); 

      Serial.println("[BACA] Mengambil status pompa dari test/pumps...");

      if (Firebase.RTDB.getJSON(&fbdo, "test/pumps")) {
        FirebaseJson &jsonResult = fbdo.jsonObject();
        FirebaseJsonData jsonData;

        jsonResult.get(jsonData, "mainPump");
        if (jsonData.success) {
          int p1 = jsonData.to<int>();
          digitalWrite(RELAY_PUMP_1, p1); 
          dummyPompa1 = (p1 == 1) ? "ON" : "OFF";
        }

        jsonResult.get(jsonData, "reservoirPump");
        if (jsonData.success) {
          int p2 = jsonData.to<int>();
          digitalWrite(RELAY_PUMP_2, p2); 
          dummyPompa2 = (p2 == 1) ? "ON" : "OFF";
        }

        Serial.println("[BACA] Berhasil menyelaraskan fisik relay.");
      } else {
        Serial.print("[BACA] Gagal: "); Serial.println(fbdo.errorReason());
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