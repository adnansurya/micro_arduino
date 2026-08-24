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

// 2. Konfigurasi Sensor Suhu (DS18B20) - MENGGUNAKAN PIN 13 DAN 25
#define ONE_WIRE_BUS_1 13  // Pin Sensor Suhu Main Tank
#define ONE_WIRE_BUS_2 25  // Pin Sensor Suhu Reservoir Tank (Aman dari Wi-Fi & Booting)

OneWire oneWire1(ONE_WIRE_BUS_1);
DallasTemperature sensors1(&oneWire1);

OneWire oneWire2(ONE_WIRE_BUS_2);
DallasTemperature sensors2(&oneWire2);

// 3. Konfigurasi Sensor Jarak (HC-SR04)
#define TRIG_PIN_1 12
#define ECHO_PIN_1 14
#define TRIG_PIN_2 27
#define ECHO_PIN_2 26

// 4. Konfigurasi Sensor pH Asli (GPIO 35)
const int phPin = 35;

// Variabel Konfigurasi Tinggi Sensor & Ketinggian Air
float mainSensorHeight = 0.0;
float reservoirSensorHeight = 0.0;
float tinggiAir1 = 0.0;
float tinggiAir2 = 0.0;

// Konfigurasi Batas Minimum Air dari Firebase
float mainMinWaterLevel = 0.0;
float reservoirMinWaterLevel = 0.0;

// Variabel Konfigurasi Waktu & Durasi Feeding Dynamic
String feedingTime = "07:20, 21:20";
float delayFeeder = 0.0;  // Durasi Murni dalam milisekon (ms)
int hariTerakhirReset = -1;

// Parameter Baru Ikan dari Firebase Config
String startingDate = "01/01/2026";
int fishCount = 0;

int menitTerakhirFeeding = -1;  // Mencegah feeder memicu berulang kali dalam 1 menit yang sama

// Status Tracking untuk Darurat pH dan Air
bool statusDaruratPH = false;
bool statusDaruratAir = false;

// Variabel Tracking Status Feeder untuk LCD
String feederStatusStr = "OFF";

// 5. Konfigurasi Pin Relay
#define RELAY_PUMP_1 4
#define RELAY_PUMP_2 5
#define RELAY_LIGHTING 18
#define SIGNAL_FEEDER 2

// 6. Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Data Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Konfigurasi NTP Server (WITA UTC+8)
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;
const int daylightOffset_sec = 0;

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

// Variabel Sensor Fisik & Real
float suhu1 = 0, suhu2 = 0;
float jarak1 = 0, jarak2 = 0;
float nilaiPH = 7.00;
String dummyPompa1 = "OFF";
String dummyPompa2 = "OFF";

// FUNGSI HELPER: Dapatkan Jumlah Sesi Feeding
int hitungJumlahSesiFeeding(String str) {
  if (str.length() == 0) return 0;
  int count = 1;
  for (int i = 0; i < str.length(); i++) {
    if (str.charAt(i) == ',') count++;
  }
  return count;
}

// FUNGSI KALKULASI DURASI FEEDER DINAMIS (RUMUS BIOMASSA)
float durasiAlat(int day, int totalIkan) {
  const float fr = 0.05;
  const float debitAlat = 0.06;

  float beratPerEkor = 0.0;

  if (day >= 5) {
    beratPerEkor = 0.0003;
    for (int i = 0; i < day - 4; i++) {
      if (i == 0) {
        continue;
      } else {
        beratPerEkor = beratPerEkor * 1.0409;
      }
    }
  }

  int jumlahSesi = hitungJumlahSesiFeeding(feedingTime);
  if (jumlahSesi < 1) jumlahSesi = 1;

  float biomassa = totalIkan * beratPerEkor;
  float pakanPerHari = biomassa * fr;
  float porsiPerSesi = pakanPerHari / (float)jumlahSesi;
  float durasi = porsiPerSesi / debitAlat;  // Durasi dalam Detik

  Serial.print("[CALC] Hari ke-");
  Serial.println(day);
  Serial.print("[CALC] Jumlah Sesi Makan: ");
  Serial.println(jumlahSesi);
  Serial.print("[CALC] Berat per Ekor: ");
  Serial.println(beratPerEkor, 6);
  Serial.print("[CALC] Biomassa: ");
  Serial.println(biomassa, 6);
  Serial.print("[CALC] Durasi per Sesi (Detik): ");
  Serial.println(durasi, 4);

  return durasi;
}

// FUNGSI MENGHITUNG SELISIH HARI DARI TANGGAL START ("DD/MM/YYYY")
int hitungHari(String dateStr) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return 1;

  int dayStart = 0, monthStart = 0, yearStart = 0;
  sscanf(dateStr.c_str(), "%d/%d/%d", &dayStart, &monthStart, &yearStart);

  struct tm startTm = { 0 };
  startTm.tm_mday = dayStart;
  startTm.tm_mon = monthStart - 1;
  startTm.tm_year = yearStart - 1900;

  time_t tStart = mktime(&startTm);
  time_t tNow;
  time(&tNow);

  double diffSeconds = difftime(tNow, tStart);
  int diffDays = (int)(diffSeconds / (60 * 60 * 24));

  if (diffDays < 1) diffDays = 1;
  return diffDays;
}

// FUNGSI UPDATE FEEDER DELAY OTOMATIS (PRESISI FLOAT 4 DESIMAL)
void kalkulasiDelayFeederOtomatis() {
  int totalHari = hitungHari(startingDate);
  float durasiDetik = durasiAlat(totalHari, fishCount);

  // Konversi murni detik ke milidetik (ms) dengan presisi float
  delayFeeder = durasiDetik * 1000.0;

  Serial.print("[FEEDER] Delay Feeder Murni Diperbarui: ");
  Serial.print(delayFeeder, 4);
  Serial.println(" ms");
}

// FUNGSI KALKULASI RUMUS PH REGRESI LINIER
float hitungPH(int adcRaw) {
  float phHasil = (adcRaw * -0.0064) + 20.21;
  if (phHasil < 0.0) phHasil = 0.0;
  if (phHasil > 14.0) phHasil = 14.0;
  return phHasil;
}

unsigned long getEpochTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return 0;
  time(&now);
  return now;
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
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
    if (currentLightingLevel >= 9) {
      currentLightingLevel = 9;
      lightingDirectionUp = false;
    }
  } else {
    currentLightingLevel--;
    if (currentLightingLevel <= 0) {
      currentLightingLevel = 0;
      lightingDirectionUp = true;
    }
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
  feederStatusStr = "ON ";  // Tandai feeder sedang aktif
  Serial.print("[FEEDER] Mengaktifkan Feeder. Durasi: ");
  Serial.print(delayFeeder, 4);
  Serial.println(" ms");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FEEDING TIME!");

  lcd.setCursor(0, 1);
  lcd.print(delayFeeder, 4);
  lcd.print("ms");

  digitalWrite(SIGNAL_FEEDER, LOW);
  delay((unsigned long)delayFeeder);
  digitalWrite(SIGNAL_FEEDER, HIGH);

  feederStatusStr = "OFF";  // Kembalikan status feeder ke OFF setelah selesai
  delay(2000);
  lcd.clear();
}

void printDebugData() {
  Serial.println("\n=== [DEBUG] DATA VARIABEL TERKINI ===");
  printLocalTime();
  Serial.print("Nilai Suhu Main & Reservoir   : ");
  Serial.print(suhu1, 1);
  Serial.print(" C | ");
  Serial.print(suhu2, 1);
  Serial.println(" C");
  Serial.print("Nilai pH Real Sensor           : ");
  Serial.println(nilaiPH, 2);
  Serial.print("Status Emergency Mode pH       : ");
  Serial.println(statusDaruratPH ? "AKTIF" : "STANDBY");
  Serial.print("Status Emergency Mode Air      : ");
  Serial.println(statusDaruratAir ? "DANGER (LOW WATER)" : "AMAN");
  Serial.print("Tanggal Mulai & Jumlah Ikan    : ");
  Serial.print(startingDate);
  Serial.print(" | ");
  Serial.print(fishCount);
  Serial.println(" ekor");
  Serial.print("Jadwal Feeding Terpasang       : ");
  Serial.println(feedingTime);
  Serial.print("Durasi Delay Feeder Hasil Calc : ");
  Serial.print(delayFeeder);
  Serial.println(" ms");
  Serial.println("======================================");
}

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

  // 1. Amankan Pin Relay & Input Utama Terlebih Dahulu
  pinMode(OTA_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(RELAY_PUMP_1, OUTPUT);
  pinMode(RELAY_PUMP_2, OUTPUT);
  pinMode(RELAY_LIGHTING, OUTPUT);
  pinMode(SIGNAL_FEEDER, OUTPUT);

  digitalWrite(RELAY_PUMP_1, HIGH);
  digitalWrite(RELAY_PUMP_2, HIGH);
  digitalWrite(RELAY_LIGHTING, HIGH);
  digitalWrite(SIGNAL_FEEDER, HIGH);

  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);

  // Inisialisasi Dua Sensor Suhu Terpisah (Pin 13 & 25)
  sensors1.begin();
  sensors2.begin();

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, panahAtas);
  lcd.createChar(1, panahBawah);

  // WELCOME SCREEN
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("AQUATRON APP");
  delay(3000);

  // 2. Jalankan WiFiManager
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Memulai WiFi...");

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  lcd.setCursor(0, 1);
  lcd.print("Cek AP: ESP32...");

  if (!wm.autoConnect("ESP32_Aquatron_AP")) {
    Serial.println("Gagal konek WiFi, merestart...");
    ESP.restart();
  }

  Serial.println("\nTersambung ke Wi-Fi!");
  lcd.clear();
  lcd.print("WiFi Terhubung!");

  // 3. Aktifkan Pin pH
  pinMode(phPin, INPUT);
  delay(500);

  // PENGECEKAN MODE OTA
  if (digitalRead(OTA_TRIGGER_PIN) == LOW) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   MODE OTA   ");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());

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

    ESP.restart();
  }

  lcd.setCursor(0, 1);
  lcd.print("Sinkron WITA...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) {
    Serial.print(".");
    delay(500);
    retry++;
  }
  Serial.println("");

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  config.timeout.serverResponse = 5000;

  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  // LOAD ALL CONFIG DALAM SATU JSON BESAR
  lcd.clear();
  lcd.print("Load Config...");
  Serial.println("\n[CONFIG] Mengunduh seluruh node /test/config dalam 1 JSON...");

  if (Firebase.RTDB.getJSON(&fbdo, "test/config")) {
    FirebaseJson &jsonResult = fbdo.jsonObject();
    FirebaseJsonData jsonData;

    jsonResult.get(jsonData, "mainSensorHeight");
    if (jsonData.success) mainSensorHeight = jsonData.to<float>();
    else mainSensorHeight = 50.0;

    jsonResult.get(jsonData, "reservoirSensorHeight");
    if (jsonData.success) reservoirSensorHeight = jsonData.to<float>();
    else reservoirSensorHeight = 50.0;

    jsonResult.get(jsonData, "mainMinWaterLevel");
    if (jsonData.success) mainMinWaterLevel = jsonData.to<float>();
    else mainMinWaterLevel = 10.0;

    jsonResult.get(jsonData, "reservoirMinWaterLevel");
    if (jsonData.success) reservoirMinWaterLevel = jsonData.to<float>();
    else reservoirMinWaterLevel = 10.0;

    jsonResult.get(jsonData, "feedingTime");
    if (jsonData.success) feedingTime = jsonData.to<String>();
    else feedingTime = "07:20, 21:20";

    jsonResult.get(jsonData, "startingDate");
    if (jsonData.success) startingDate = jsonData.to<String>();
    else startingDate = "17/08/2026";

    jsonResult.get(jsonData, "fishCount");
    if (jsonData.success) fishCount = jsonData.to<int>();
    else fishCount = 30;

  } else {
    Serial.print("[CONFIG] Gagal mengambil JSON Config: ");
    Serial.println(fbdo.errorReason());
    mainSensorHeight = 50.0;
    reservoirSensorHeight = 50.0;
    mainMinWaterLevel = 10.0;
    reservoirMinWaterLevel = 10.0;
    feedingTime = "07:20, 21:20";
    startingDate = "17/08/2026";
    fishCount = 30;
  }

  // Hitung delayFeeder otomatis murni tanpa batas bawah
  kalkulasiDelayFeederOtomatis();

  if (getLocalTime(&timeinfo)) {
    hariTerakhirReset = timeinfo.tm_mday;
  }

  // ==========================================
  // MENAMPILKAN INFO FEEDER & DELAY DENGAN 4 DESIMAL
  // ==========================================
  int totalHari = hitungHari(startingDate);

  // Layar 1: Info Hari Ke-X & Jumlah Ikan
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DAY:");
  lcd.print(totalHari);
  lcd.print(" FISH:");
  lcd.print(fishCount);
  lcd.setCursor(0, 1);
  lcd.print(feedingTime.substring(0, 15));
  delay(5000);

  // Layar 2: Info Feeder Delay 4 Desimal (ms)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FEEDER DELAY:");
  lcd.setCursor(0, 1);
  lcd.print(delayFeeder, 4);  // Menampilkan 4 angka di belakang koma
  lcd.print("ms");
  delay(5000);

  lcd.clear();
  firebasePrevMillis = millis() - firebaseInterval;
  sinkronisasiLighting();

  lcd.clear();
  firebasePrevMillis = millis() - firebaseInterval;
  sinkronisasiLighting();
}

void loop() {
  struct tm timeinfo;

  // ==========================================
  // 1. MEMBACA DATA SENSOR REAL DARI 2 PIN SUHU
  // ==========================================
  sensors1.requestTemperatures();
  suhu1 = sensors1.getTempCByIndex(0);  // Main Tank (GPIO 13)

  sensors2.requestTemperatures();
  suhu2 = sensors2.getTempCByIndex(0);  // Reservoir Tank (GPIO 25)

  jarak1 = bacaJarak(TRIG_PIN_1, ECHO_PIN_1);
  jarak2 = bacaJarak(TRIG_PIN_2, ECHO_PIN_2);

  if (jarak1 != -1) {
    tinggiAir1 = mainSensorHeight - jarak1;
    if (tinggiAir1 < 0) tinggiAir1 = 0;
  } else {
    tinggiAir1 = -1;
  }

  if (jarak2 != -1) {
    tinggiAir2 = reservoirSensorHeight - jarak2;
    if (tinggiAir2 < 0) tinggiAir2 = 0;
  } else {
    tinggiAir2 = -1;
  }

  long phSum = 0;
  int phSamples = 10;
  for (int i = 0; i < phSamples; i++) {
    phSum += analogRead(phPin);
    delay(10);
  }
  int adcValueReal = phSum / phSamples;
  nilaiPH = hitungPH(adcValueReal);

  bool airMainLow = (tinggiAir1 != -1 && tinggiAir1 < mainMinWaterLevel);
  bool airReservoirLow = (tinggiAir2 != -1 && tinggiAir2 < reservoirMinWaterLevel);
  if (airMainLow || airReservoirLow) statusDaruratAir = true;
  else statusDaruratAir = false;

  // ==========================================
  // 2. CHECK PERUBAHAN JAM & BERGANTI HARI & MULTI FEEDING JADWAL
  // ==========================================
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_hour != lastCheckedHour) {
      lastCheckedHour = timeinfo.tm_hour;
      sinkronisasiLighting();
    }

    char jamSekarangStr[6];
    sprintf(jamSekarangStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    // Pergantian hari: hitung ulang biomassa pakan & reset status makan harian ke 0
    if (timeinfo.tm_mday != hariTerakhirReset) {
      Serial.println("[FEEDER] Hari berganti. Perbarui delayFeeder & Reset feedingToday...");
      kalkulasiDelayFeederOtomatis();

      if (Firebase.RTDB.setInt(&fbdo, "test/feeder/feedingToday", 0)) {
        hariTerakhirReset = timeinfo.tm_mday;
      }
    }

    // LOGIKA MULTI-JADWAL FEEDING
    int indexSesi = 0;
    // ==========================================
    // LOGIKA PELEPASAN PAKAN OTOMATIS (EXACT MATCH JADWAL)
    // ==========================================


    // Hanya jalankan jika belum pernah feeding di menit yang sama
    if (timeinfo.tm_min != menitTerakhirFeeding) {
      int startIdx = 0;
      int strLen = feedingTime.length();

      while (startIdx < strLen) {
        int commaIdx = feedingTime.indexOf(',', startIdx);
        if (commaIdx == -1) commaIdx = strLen;

        String subTime = feedingTime.substring(startIdx, commaIdx);
        subTime.trim();  // Bersihkan spasi jika ada format "07:20, 21:20"

        // Format standar 2 digit jam (contoh "7:20" -> "07:20")
        if (subTime.length() == 4 && subTime.charAt(1) == ':') {
          subTime = "0" + subTime;
        }

        // JIKA WAKTU SAAT INI COCOK DENGAN DASHBOARD / JADWAL
        if (subTime.equalsIgnoreCase(jamSekarangStr)) {
          Serial.print("[FEEDER] Waktu cocok dengan jadwal: ");
          Serial.println(subTime);

          // 1. Catat menit agar tidak memicu berulang kali dalam 60 detik ini
          menitTerakhirFeeding = timeinfo.tm_min;

          // 2. Kalkulasi ulang durasi feeder sesuai pertumbuhan biomassa ikan terbaru
          kalkulasiDelayFeederOtomatis();

          // 3. Eksekusi menyalakan feeder & update layar LCD
          jalankanFeeder();

          // 4. Update status bitmasking di Firebase (opsional/tetap dipertahankan)
          if (Firebase.RTDB.getInt(&fbdo, "test/feeder/feedingToday")) {
            int maskFeeding = fbdo.to<int>();
            int bitCheck = (1 << indexSesi);
            maskFeeding |= bitCheck;
            Firebase.RTDB.setInt(&fbdo, "test/feeder/feedingToday", maskFeeding);
          }

          break;  // Keluar dari loop pencarian jadwal setelah feeder dijalankan
        }

        startIdx = commaIdx + 1;
      }
    }
  }

  // ==========================================
  // 3. LOGIKA PERGANTIAN HALAMAN LCD
  // ==========================================
  bool perluUpdateLayar = false;
  if (millis() - changePagePrevMillis > pageInterval) {
    changePagePrevMillis = millis();
    currentPage++;

    // Perbarui batas maksimum halaman menjadi 6
    if (currentPage > 6) currentPage = 0;
    if (currentPage == 6 && !statusDaruratPH && !statusDaruratAir) currentPage = 0;

    lcd.clear();
    perluUpdateLayar = true;
  }

  if (perluUpdateLayar) {
    switch (currentPage) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print("TEMPERATURE");
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
        lcd.print("WATER LEVEL");
        lcd.setCursor(0, 1);
        if (tinggiAir1 == -1) lcd.print("M:ERR ");
        else {
          lcd.print("M:");
          lcd.print(tinggiAir1, 0);
          lcd.print("cm ");
        }
        lcd.setCursor(9, 1);
        if (tinggiAir2 == -1) lcd.print("R:ERR");
        else {
          lcd.print("R:");
          lcd.print(tinggiAir2, 0);
          lcd.print("cm");
        }
        break;

      case 2:
        lcd.setCursor(0, 0);
        lcd.print("PH METER");
        lcd.setCursor(0, 1);
        lcd.print("Nilai pH : ");
        lcd.print(nilaiPH, 2);
        break;

      case 3:
        lcd.setCursor(0, 0);
        lcd.print("PUMP STATUS");
        lcd.setCursor(0, 1);
        lcd.print("M:");
        lcd.print(dummyPompa1);
        lcd.setCursor(9, 1);
        lcd.print("R:");
        lcd.print(dummyPompa2);
        break;

      case 4:
        lcd.setCursor(0, 0);
        lcd.print("LIGHTING SYSTEM");
        lcd.setCursor(0, 1);
        lcd.print("Current Lvl: ");
        lcd.print(currentLightingLevel);
        break;

      // === HALAMAN BARU: FEEDER STATUS ===
      case 5:
        {
          // Tampilkan Jam:Menit Terkini dan Status Feeder (ON/OFF)
          lcd.setCursor(0, 0);
          if (getLocalTime(&timeinfo)) {
            char timeBuf[6];
            sprintf(timeBuf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            lcd.print(">");
            lcd.print(timeBuf);
          } else {
            lcd.print("--:--");
          }

          // Atur posisi status "ON " atau "OFF" di pojok kanan atas
          lcd.print(" Feed:");
          lcd.print(feederStatusStr);

          // Tampilkan Jadwal Feeding di Baris Keduas
          lcd.setCursor(0, 1);
          if (feedingTime.length() > 16) {
            lcd.print(feedingTime.substring(0, 16));  // Potong jika lebih dari 16 karakter
          } else {
            lcd.print(feedingTime);
          }
        }
        break;

      // Halaman Warning digeser ke case 6
      case 6:
        lcd.setCursor(0, 0);
        lcd.print("!! WARNING !!");
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
    if (iconStatus == 1) {
      lcd.setCursor(15, 0);
      lcd.write(0);
    } else if (iconStatus == 2) {
      lcd.setCursor(15, 0);
      lcd.write(1);
    }
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
    printDebugData();

    if (toggleTask) {
      iconStatus = 1;
      lcd.setCursor(15, 0);
      lcd.write(0);
      float selisihSuhu = abs(suhu1 - suhu2);
      bool phAbnormal = (nilaiPH < 6.7 || nilaiPH > 7.3);
      bool suhuStabil = (suhu1 != DEVICE_DISCONNECTED_C && suhu2 != DEVICE_DISCONNECTED_C && selisihSuhu <= 0.5);

      // Syarat Ketinggian air kedua tangki harus > 10 cm
      bool airCukupUntukPompa = (tinggiAir1 > 10.0 && tinggiAir2 > 10.0);

      if (!statusDaruratPH) {
        // Mode darurat AKTIF jika: pH abnormal + Suhu stabil + Air kedua tangki > 10 cm
        if (phAbnormal && suhuStabil && airCukupUntukPompa) {
          statusDaruratPH = true;
          FirebaseJson jsonPumps;
          jsonPumps.set("mainPump", 1);
          jsonPumps.set("reservoirPump", 1);
          Firebase.RTDB.updateNode(&fbdo, "test/pumps", &jsonPumps);
          digitalWrite(RELAY_PUMP_1, LOW);
          digitalWrite(RELAY_PUMP_2, LOW);
          dummyPompa1 = "ON";
          dummyPompa2 = "ON";
        }
      } else {
        // Mode darurat MATI jika: pH sudah normal OR air <= 10 cm OR suhu TIDAK stabil
        if (!phAbnormal || !airCukupUntukPompa || !suhuStabil) {
          statusDaruratPH = false;
          FirebaseJson jsonPumps;
          jsonPumps.set("mainPump", 0);
          jsonPumps.set("reservoirPump", 0);
          Firebase.RTDB.updateNode(&fbdo, "test/pumps", &jsonPumps);
          digitalWrite(RELAY_PUMP_1, HIGH);
          digitalWrite(RELAY_PUMP_2, HIGH);
          dummyPompa1 = "OFF";
          dummyPompa2 = "OFF";
        }
      }

      FirebaseJson json;
      if (suhu1 != DEVICE_DISCONNECTED_C) json.set("mainTemperature", suhu1);
      if (tinggiAir1 != -1) json.set("mainWaterLevel", tinggiAir1);
      if (suhu2 != DEVICE_DISCONNECTED_C) json.set("reservoirTemperature", suhu2);
      if (tinggiAir2 != -1) json.set("reservoirWaterLevel", tinggiAir2);
      json.set("ph", nilaiPH);
      json.set("lightingLevel", currentLightingLevel);

      unsigned long currentEpoch = getEpochTime();
      if (currentEpoch != 0) json.set("updatedAt", currentEpoch);

      String pathLatest = "test/sensorData/aquatron_001/latest";
      Firebase.RTDB.updateNode(&fbdo, pathLatest, &json);

      String pathHistory = "test/history";
      Firebase.RTDB.push(&fbdo, pathHistory, &json);

    } else {
      iconStatus = 2;
      lcd.setCursor(15, 0);
      lcd.write(1);
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
    iconStatus = 0;
    lcd.setCursor(15, 0);
    lcd.print(" ");
  }
}