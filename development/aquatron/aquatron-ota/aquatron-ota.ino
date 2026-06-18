#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>
#include <time.h> 

// === TAMBAHAN OTA 1: Library untuk Web Server dan OTA ===
#include <WebServer.h>
#include <ElegantOTA.h>

// Helper code untuk Firebase data logging dan token generation
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// 1. Konfigurasi Firebase
#define FIREBASE_HOST "https://aquatron-app-default-rtdb.asia-southeast1.firebasedatabase.app/" 
#define FIREBASE_AUTH "PacMY6HzqqijY7X44xesrqZG8cd1sCsuQNs47JgG"

// === TAMBAHAN OTA 2: Definisi Pin Trigger OTA ===
#define OTA_TRIGGER_PIN 19
WebServer server(80); 

// 2. Konfigurasi Sensor Suhu (DS18B20)
#define ONE_WIRE_BUS 13
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// 3. Konfigurasi Sensor Jarak (HC-SR04)
#define TRIG_PIN_1 12
#define ECHO_PIN_1 14
#define TRIG_PIN_2 27
#define ECHO_PIN_2 26

// Variabel Konfigurasi Tinggi Sensor & Ketinggian Air
float mainSensorHeight = 0.0;       
float reservoirSensorHeight = 0.0;  
float tinggiAir1 = 0.0;             
float tinggiAir2 = 0.0;             

// Konfigurasi Batas Minimum Air dari Firebase
float mainMinWaterLevel = 20.0;
float reservoirMinWaterLevel = 20.0;

// Variabel Konfigurasi Waktu Feeding
String feedingTime = "00:00"; 
int hariTerakhirReset = -1; // Variabel lokal untuk melacak pergantian hari untuk reset Firebase

// Status Tracking untuk Darurat pH dan Air
bool statusDaruratPH = false; 
bool statusDaruratAir = false; 

// 4. Konfigurasi Pin Relay 
#define RELAY_PUMP_1 4   
#define RELAY_PUMP_2 5   
#define RELAY_LIGHTING 18  
#define RELAY_FEEDER 2     

// 5. Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Data Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Konfigurasi NTP Server (WITA UTC+8)
const char* ntpServer = "pool.ntp.org";
const long   gmtOffset_sec = 28800;     
const int    daylightOffset_sec = 0;

// Tabel Kesepakatan Jam
const int tabelLighting[24] = {
  9, 9, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 
  0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9  
};

// Variabel Tracking Level Lighting Fisik
int currentLightingLevel = 0; 
bool lightingDirectionUp = true; 
int lastCheckedHour = -1;        

// Management Waktu Non-blocking
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

// Byte kustom untuk karakter panah
byte panahAtas[8] = { B00100, B01110, B11111, B00100, B00100, B00100, B00100, B00100 };
byte panahBawah[8] = { B00100, B00100, B00100, B00100, B00100, B11111, B01110, B00100 };

// Variabel Sensor & Dummy
float suhu1 = 0, suhu2 = 0;
float jarak1 = 0, jarak2 = 0;
float dummyPH = 7.00;              
String dummyPompa1 = "OFF";         
String dummyPompa2 = "OFF";         

unsigned long getEpochTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return 0; 
  time(&now);
  return now;
}

void printLocalTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return;
  Serial.print("Waktu Sistem Saat Ini (WITA): ");
  Serial.println(&timeinfo, "%A, %d/%m/%Y %H:%M:%S");
}

void tekanTombolLighting() {
  Serial.println("[LIGHTING] Relay 3 LOW (Menekan Tombol...)");
  digitalWrite(RELAY_LIGHTING, LOW);  
  delay(200);                         
  digitalWrite(RELAY_LIGHTING, HIGH); 
  delay(200);                         

  if (lightingDirectionUp) {
    currentLightingLevel++;
    if (currentLightingLevel >= 9) { currentLightingLevel = 9; lightingDirectionUp = false; }
  } else {
    currentLightingLevel--;
    if (currentLightingLevel <= 0) { currentLightingLevel = 0; lightingDirectionUp = true; }
  }
}

void sinkronisasiLighting() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  int jamSekarang = timeinfo.tm_hour;
  int levelTarget = tabelLighting[jamSekarang];
  while (currentLightingLevel != levelTarget) {
    tekanTombolLighting();
    yield(); 
  }
}

void jalankanFeeder() {
  Serial.println("[FEEDER] Mengaktifkan Feeder pakan ikan...");
  
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(" TIME TO FEED! ");
  lcd.setCursor(0, 1); lcd.print("  GIVE ME FOOD  ");
  
  digitalWrite(RELAY_FEEDER, LOW);  
  delay(3000);                      
  digitalWrite(RELAY_FEEDER, HIGH); 
  
  lcd.clear();
}

void printDebugData() {
  struct tm timeinfo;
  int jamSekarang = 0;
  if (getLocalTime(&timeinfo)) jamSekarang = timeinfo.tm_hour;
  Serial.println("\n=== [DEBUG] DATA VARIABEL TERKINI ===");
  printLocalTime(); 
  Serial.print("Status Emergency Mode pH       : "); Serial.println(statusDaruratPH ? "AKTIF" : "STANDBY");
  Serial.print("Status Emergency Mode Air      : "); Serial.println(statusDaruratAir ? "DANGER (LOW WATER)" : "AMAN");
  Serial.print("Jadwal Feeding Terpasang       : "); Serial.println(feedingTime);
  Serial.println("======================================");
}

float bacaJarak(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  float distance = duration * 0.034 / 2;
  if (distance == 0) return -1;
  return distance;
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(34)); 

  pinMode(OTA_TRIGGER_PIN, INPUT_PULLUP);

  pinMode(RELAY_PUMP_1, OUTPUT);
  pinMode(RELAY_PUMP_2, OUTPUT);
  pinMode(RELAY_LIGHTING, OUTPUT);
  pinMode(RELAY_FEEDER, OUTPUT);    
  
  digitalWrite(RELAY_PUMP_1, HIGH); 
  digitalWrite(RELAY_PUMP_2, HIGH);
  digitalWrite(RELAY_LIGHTING, HIGH); 
  digitalWrite(RELAY_FEEDER, HIGH);  

  pinMode(TRIG_PIN_1, OUTPUT); pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT); pinMode(ECHO_PIN_2, INPUT);

  sensors.begin();
  lcd.init(); lcd.backlight();
  lcd.createChar(0, panahAtas); lcd.createChar(1, panahBawah); 
  
  lcd.setCursor(0, 0); lcd.print("Memulai WiFi...");

  WiFiManager wm;
  lcd.setCursor(0, 1); lcd.print("Cek AP: ESP32...");
  
  if (!wm.autoConnect("ESP32_Aquatron_AP")) {
    Serial.println("Gagal konek WiFi, merestart...");
    ESP.restart();
  }

  Serial.println("\nTersambung ke Wi-Fi!");
  lcd.clear(); lcd.print("WiFi Terhubung!");
  
  // PENGECEKAN MODE OTA TEPAT SETELAH WIFI TERHUBUNG
  if (digitalRead(OTA_TRIGGER_PIN) == LOW) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("   MODE OTA   ");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
    Serial.print("\n[OTA] Masuk Mode OTA Statis! Buka: http://");
    Serial.println(WiFi.localIP());

    server.on("/", []() {
      server.send(200, "text/plain", "ESP32 Mode OTA Aktif setelah Booting Wi-Fi.");
    });
    ElegantOTA.begin(&server);    
    server.begin();

    while (digitalRead(OTA_TRIGGER_PIN) == LOW) {
      server.handleClient();
      ElegantOTA.loop();
      delay(1);
    }
    
    Serial.println("[OTA] Pin dilepas, merestart ESP32 ke mode normal...");
    lcd.clear(); lcd.print("Restarting...");
    delay(1000);
    ESP.restart();
  }

  lcd.setCursor(0, 1); lcd.print("Sinkron WITA...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  int retry = 0;
  while(!getLocalTime(&timeinfo) && retry < 10) {
    Serial.print("."); delay(500); retry++;
  }
  Serial.println("");
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  config.timeout.serverResponse = 5000;  
  
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  // LOAD ALL CONFIG DALAM SATU JSON BESAR (SINGLE FETCH)
  lcd.clear(); lcd.print("Load Config...");
  Serial.println("\n[CONFIG] Mengunduh seluruh node /test/config dalam 1 JSON...");

  if (Firebase.RTDB.getJSON(&fbdo, "test/config")) {
    FirebaseJson &jsonResult = fbdo.jsonObject();
    FirebaseJsonData jsonData;

    jsonResult.get(jsonData, "mainSensorHeight");
    if (jsonData.success) mainSensorHeight = jsonData.to<float>(); else mainSensorHeight = 50.0;

    jsonResult.get(jsonData, "reservoirSensorHeight");
    if (jsonData.success) reservoirSensorHeight = jsonData.to<float>(); else reservoirSensorHeight = 50.0;

    jsonResult.get(jsonData, "mainMinWaterLevel");
    if (jsonData.success) mainMinWaterLevel = jsonData.to<float>(); else mainMinWaterLevel = 10.0;

    jsonResult.get(jsonData, "reservoirMinWaterLevel");
    if (jsonData.success) reservoirMinWaterLevel = jsonData.to<float>(); else reservoirMinWaterLevel = 10.0;

    jsonResult.get(jsonData, "feedingTime");
    if (jsonData.success) {
      feedingTime = jsonData.to<String>();
      Serial.print("[CONFIG] Dipisah -> feedingTime: "); Serial.println(feedingTime);
    } else { 
      feedingTime = "08:00"; 
    }
  } 
  else {
    Serial.print("[CONFIG] Gagal mengambil JSON Config: "); Serial.println(fbdo.errorReason());
    mainSensorHeight = 50.0; reservoirSensorHeight = 50.0;
    mainMinWaterLevel = 10.0; reservoirMinWaterLevel = 10.0;
    feedingTime = "08:00";
  }

  // Set nilai awal hari reset berdasarkan tanggal booting saat ini
  if (getLocalTime(&timeinfo)) {
    hariTerakhirReset = timeinfo.tm_mday;
  }

  lcd.clear();
  firebasePrevMillis = millis() - firebaseInterval; 
  sinkronisasiLighting();
}

void loop() {
  struct tm timeinfo; 

  // ==========================================
  // 1. MEMBACA DATA SENSOR INTERNAL ESP32 & KALKULASI TINGGI AIR
  // ==========================================
  sensors.requestTemperatures();
  suhu1 = sensors.getTempCByIndex(0);
  suhu2 = sensors.getTempCByIndex(1);
  
  jarak1 = bacaJarak(TRIG_PIN_1, ECHO_PIN_1);
  jarak2 = bacaJarak(TRIG_PIN_2, ECHO_PIN_2);

  if (jarak1 != -1) {
    tinggiAir1 = mainSensorHeight - jarak1;
    if (tinggiAir1 < 0) tinggiAir1 = 0; 
  } else { tinggiAir1 = -1; }

  if (jarak2 != -1) {
    tinggiAir2 = reservoirSensorHeight - jarak2;
    if (tinggiAir2 < 0) tinggiAir2 = 0;
  } else { tinggiAir2 = -1; }

  // Evaluasi Batas Air Minimum
  bool airMainLow = (tinggiAir1 != -1 && tinggiAir1 < mainMinWaterLevel);
  bool airReservoirLow = (tinggiAir2 != -1 && tinggiAir2 < reservoirMinWaterLevel);
  
  if (airMainLow || airReservoirLow) statusDaruratAir = true; else statusDaruratAir = false;

  // ==========================================
  // 2. CHECK PERUBAHAN JAM (LIGHTING & FEEDER)
  // ==========================================
  if (getLocalTime(&timeinfo)) {
    // A. Sinkronisasi Jam untuk Lampu
    if (timeinfo.tm_hour != lastCheckedHour) {
      lastCheckedHour = timeinfo.tm_hour; 
      sinkronisasiLighting();            
    }

    // B. LOGIKA PENGECEKAN FEEDER VIA FIREBASE REAL-TIME
    char jamSekarangStr[6];
    sprintf(jamSekarangStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    
    // Reset status di Firebase jika hari berganti (Jam 00:00 ke atas)
    if (timeinfo.tm_mday != hariTerakhirReset) {
      Serial.println("[FEEDER] Hari berganti. Reset status feedingToday di Firebase ke 0...");
      if (Firebase.RTDB.setInt(&fbdo, "test/feeder/feedingToday", 0)) {
        hariTerakhirReset = timeinfo.tm_mday;
      }
    }

    // Cek Firebase tepat ketika waktu feeding tiba
    if (feedingTime.equalsIgnoreCase(jamSekarangStr)) {
      // Ambil nilai feedingToday dari Firebase secara real-time
      if (Firebase.RTDB.getInt(&fbdo, "test/feeder/feedingToday")) {
        int statusFeeding = fbdo.to<int>();
        
        // Jika bernilai 0 (artinya belum diberi makan hari ini)
        if (statusFeeding == 0) {
          jalankanFeeder();
          
          // Tandai di Firebase menjadi 1 agar jika ESP32 restart tidak berulang
          Serial.println("[FEEDER] Update status feedingToday ke Firebase -> 1");
          Firebase.RTDB.setInt(&fbdo, "test/feeder/feedingToday", 1);
        }
      }
    }
  }

  // ==========================================
  // 3. LOGIKA PERGANTIAN HALAMAN LCD (2 DETIK)
  // ==========================================
  bool perluUpdateLayar = false;
  if (millis() - changePagePrevMillis > pageInterval) {
    changePagePrevMillis = millis();
    currentPage++;
    
    if (currentPage > 5) currentPage = 0; 
    if (currentPage == 5 && !statusDaruratPH && !statusDaruratAir) currentPage = 0;

    lcd.clear(); perluUpdateLayar = true; 
  }

  if (perluUpdateLayar) {
    switch (currentPage) {
      case 0:
        lcd.setCursor(0, 0); lcd.print("TEMPERATURE");
        lcd.setCursor(0, 1); lcd.print("M:"); lcd.print(suhu1, 1); lcd.print((char)223); lcd.print("C ");
        lcd.setCursor(9, 1); lcd.print("R:"); lcd.print(suhu2, 1); lcd.print((char)223); lcd.print("C");
        break;

      case 1:
        lcd.setCursor(0, 0); lcd.print("WATER LEVEL");
        lcd.setCursor(0, 1); if (tinggiAir1 == -1) lcd.print("M:ERR "); else { lcd.print("M:"); lcd.print(tinggiAir1, 0); lcd.print("cm "); }
        lcd.setCursor(9, 1); if (tinggiAir2 == -1) lcd.print("R:ERR"); else { lcd.print("R:"); lcd.print(tinggiAir2, 0); lcd.print("cm"); }
        break;

      case 2:
        lcd.setCursor(0, 0); lcd.print("PH METER");
        lcd.setCursor(0, 1); lcd.print("Nilai pH : "); lcd.print(dummyPH, 2);
        break;

      case 3:
        lcd.setCursor(0, 0); lcd.print("PUMP STATUS");
        lcd.setCursor(0, 1); lcd.print("M:"); lcd.print(dummyPompa1);
        lcd.setCursor(9, 1); lcd.print("R:"); lcd.print(dummyPompa2);
        break;

      case 4: 
        lcd.setCursor(0, 0); lcd.print("LIGHTING SYSTEM");
        lcd.setCursor(0, 1); lcd.print("Current Lvl: "); lcd.print(currentLightingLevel);
        break;
      
      case 5:
        lcd.setCursor(0, 0); lcd.print("!! WARNING !!");
        lcd.setCursor(0, 1);
        if (statusDaruratPH && statusDaruratAir) {
          lcd.print("BAD pH & LOW WTR");
        } else if (statusDaruratPH) {
          lcd.print("PH ABNORMAL! ");
        } else if (statusDaruratAir) {
          if (airMainLow && airReservoirLow) lcd.print("ALL WATER LOW!");
          else if (airMainLow) lcd.print("MAIN WATER LOW!");
          else if (airReservoirLow) lcd.print("RESV WATER LOW!");
        }
        break;
    }
    if (iconStatus == 1) { lcd.setCursor(15, 0); lcd.write(0); }
    else if (iconStatus == 2) { lcd.setCursor(15, 0); lcd.write(1); }
  }

  // ==========================================
  // 4. LOGIKA AKTIVITAS FIREBASE (SINKRONISASI & RELAY)
  // ==========================================
  if (!firebaseReadyToTrigger && (millis() - firebasePrevMillis >= firebaseInterval)) {
    firebaseReadyToTrigger = true; 
  }

  if (firebaseReadyToTrigger) {
    firebaseReadyToTrigger = false; 
    iconTurnOffMillis = millis() + 1000; 
    dummyPH = random(500, 801) / 100.0; 
    printDebugData();

    if (toggleTask) {
      iconStatus = 1; lcd.setCursor(15, 0); lcd.write(0); 
      float selisihSuhu = abs(suhu1 - suhu2);
      bool phAbnormal = (dummyPH < 6.0 || dummyPH > 7.5);
      bool suhuStabil = (suhu1 != DEVICE_DISCONNECTED_C && suhu2 != DEVICE_DISCONNECTED_C && selisihSuhu <= 0.5);

      if (!statusDaruratPH) {
        if (phAbnormal && suhuStabil) {
          statusDaruratPH = true; 
          FirebaseJson jsonPumps;
          jsonPumps.set("mainPump", 1); jsonPumps.set("reservoirPump", 1);
          Firebase.RTDB.updateNode(&fbdo, "test/pumps", &jsonPumps);
          digitalWrite(RELAY_PUMP_1, LOW); digitalWrite(RELAY_PUMP_2, LOW);
          dummyPompa1 = "ON"; dummyPompa2 = "ON";
        }
      } else {
        if (!phAbnormal) {
          statusDaruratPH = false; 
          FirebaseJson jsonPumps;
          jsonPumps.set("mainPump", 0); jsonPumps.set("reservoirPump", 0);
          Firebase.RTDB.updateNode(&fbdo, "test/pumps", &jsonPumps);
          digitalWrite(RELAY_PUMP_1, HIGH); digitalWrite(RELAY_PUMP_2, HIGH);
          dummyPompa1 = "OFF"; dummyPompa2 = "OFF";
        }
      }

      FirebaseJson json;
      if (suhu1 != DEVICE_DISCONNECTED_C) json.set("mainTemperature", suhu1);
      if (tinggiAir1 != -1) json.set("mainWaterLevel", tinggiAir1);
      if (suhu2 != DEVICE_DISCONNECTED_C) json.set("reservoirTemperature", suhu2);
      if (tinggiAir2 != -1) json.set("reservoirWaterLevel", tinggiAir2);
      json.set("ph", dummyPH);
      json.set("lightingLevel", currentLightingLevel); 
      
      unsigned long currentEpoch = getEpochTime();
      if (currentEpoch != 0) json.set("updatedAt", currentEpoch);

      String pathLatest = "test/sensorData/aquatron_001/latest";
      Firebase.RTDB.updateNode(&fbdo, pathLatest, &json);

      String pathHistory = "test/history";
      Firebase.RTDB.push(&fbdo, pathHistory, &json);

    } else {
      iconStatus = 2; lcd.setCursor(15, 0); lcd.write(1); 
      if (Firebase.RTDB.getJSON(&fbdo, "test/pumps")) {
        FirebaseJson &jsonResult = fbdo.jsonObject();
        FirebaseJsonData jsonData;
        jsonResult.get(jsonData, "mainPump");
        if (jsonData.success) {
          int p1 = jsonData.to<int>();
          digitalWrite(RELAY_PUMP_1, !p1);
          dummyPompa1 = (p1 == 1) ? "ON" : "OFF";
        }
        jsonResult.get(jsonData, "reservoirPump");
        if (jsonData.success) {
          int p2 = jsonData.to<int>();
          digitalWrite(RELAY_PUMP_2, !p2);
          dummyPompa2 = (p2 == 1) ? "ON" : "OFF";
        }
      }
    }
    firebasePrevMillis = millis(); 
    toggleTask = !toggleTask; 
  }

  if (iconStatus != 0 && millis() > iconTurnOffMillis) {
    iconStatus = 0; lcd.setCursor(15, 0); lcd.print(" "); 
  }
  delay(50); 
}