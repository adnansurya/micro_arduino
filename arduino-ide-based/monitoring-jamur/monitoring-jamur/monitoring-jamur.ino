#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define BUZZER_PIN 18

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variabel Konfigurasi Range & Delta dari Google Sheets (dengan nilai default)
float suhuMin = 22.0;
float suhuMax = 28.0;
float kelembabanMin = 70.0;
float kelembabanMax = 90.0;
float deltaSuhu = 0.5;
float deltaKelembaban = 2.0;
long intervalData = 60000;

// Variabel data sensor
float suhuSesaat = 0.0;
float kelembabanSesaat = 0.0;
float suhuTerkirim = 0.0;
float kelembabanTerkirim = 0.0;

// Variabel Penanda Waktu (millis)
unsigned long waktuTerakhirOLED = 0;
unsigned long waktuTerakhirKirim = 0;
unsigned long waktuTerakhirBuzzer = 0;
unsigned long waktuResetIkon = 0;

// Variabel Status Indikator
int statusKirimIcon = 0;
bool isOutOfRange = false;  // Penanda jika data sensor keluar dari batas aman

void fetchThresholds() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Fetching Config...");
    display.display();

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String urlWithParam = String(WEB_APP_URL) + "?action=get_config";
    http.begin(urlWithParam);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        suhuMin = doc["suhu_min"];
        suhuMax = doc["suhu_max"];
        kelembabanMin = doc["kelembaban_min"];
        kelembabanMax = doc["kelembaban_max"];
        deltaSuhu = doc["delta_suhu"];
        deltaKelembaban = doc["delta_kelembaban"];
        intervalData = doc["interval_data"].as<long>() * 1000;

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("RANGE CONFIG LOADED:");
        display.println("---------------------");
        display.printf("Suhu : %.1f - %.1f C\n", suhuMin, suhuMax);
        display.printf("Humi : %.1f - %.1f %%\n", kelembabanMin, kelembabanMax);
        display.printf("Delta T: %.1f | H: %.1f\n", deltaSuhu, deltaKelembaban);
        display.printf("Interval: %ld s\n", intervalData / 1000);
        display.display();

        Serial.println("--- CONFIG RENTANG BERHASIL DI-LOAD ---");
      } else {
        Serial.println("Gagal Parse JSON Config!");
      }
    } else {
      Serial.print("HTTP GET Config Gagal, Error: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }
    http.end();
    delay(4000);
  }
}

// Fungsi menggambar ikon pengiriman data (Ujung Kanan Bawah)
void drawStatusIcon(int status) {
  display.setTextSize(1);
  int iconX = 118;
  int iconY = 52;  // Koordinat bawah

  if (status == 1) {
    display.setCursor(iconX, iconY);
    display.print("^");
  } else if (status == 2) {
    display.drawLine(iconX, iconY + 4, iconX + 3, iconY + 7, SSD1306_WHITE);
    display.drawLine(iconX + 3, iconY + 7, iconX + 8, iconY + 1, SSD1306_WHITE);
  } else if (status == 3) {
    display.setCursor(iconX, iconY);
    display.print("X");
  }
}

// Fungsi menggambar simbol Peringatan 'o' (Hanya muncul jika diluar range DAN buzzer sedang berbunyi)
void drawWarningIcon(bool outOfRange) {
  // digitalRead(BUZZER_PIN) memastikan 'o' hanya muncul saat buzzer aktif mengeluarkan suara
  if (outOfRange && digitalRead(BUZZER_PIN) == HIGH) {
    int warnX = 118;
    int warnY = 38;
    display.setCursor(warnX, warnY);
    display.setTextSize(1);
    display.print("o");
  }
}

void kirimDataKeGoogle(float t, float h) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(WEB_APP_URL);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["suhu"] = t;
    doc["kelembaban"] = h;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    statusKirimIcon = 1;

    // Tampilkan layar dengan ikon mengirim yang aktif saat proses berjalan
    display.clearDisplay();
    // (Proses gambar ulang teks agar layar tidak berkedip kosong saat http post)
    // Header teks (Ukuran 1)
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("MONITORING SENSOR");
    display.println("---------------------");

    // --- BARIS SUHU ---
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("T:");        // Menghilangkan spasi setelah titik dua
    display.setCursor(30, 20);  // Mengunci posisi angka pas di koordinat X=22 (sangat dekat dengan ":")
    display.print(suhuSesaat, 1);

    // Simbol derajat Celcius digeser agar mengikuti angka
    display.setTextSize(1);
    display.print(" o");
    display.setTextSize(2);
    display.print("C");

    // --- BARIS KELEMBABAN ---
    display.setCursor(0, 42);
    display.print("H:");        // Menghilangkan spasi setelah titik dua
    display.setCursor(30, 42);  // Mengunci posisi angka pas di koordinat X=22
    display.print(kelembabanSesaat, 1);
    display.print(" %");

    // Gambar Ikon Pengiriman dan Ikon Warning 'o' di sisi kanan yang sudah lowong
    drawStatusIcon(statusKirimIcon);
    drawWarningIcon(isOutOfRange);

    display.display();

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      statusKirimIcon = 2;
      suhuTerkirim = t;
      kelembabanTerkirim = h;
    } else {
      statusKirimIcon = 3;
    }
    http.end();
  } else {
    statusKirimIcon = 3;
  }
  waktuResetIkon = millis();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  fetchThresholds();

  suhuSesaat = dht.readTemperature();
  kelembabanSesaat = dht.readHumidity();

  if (!isnan(suhuSesaat) && !isnan(kelembabanSesaat)) {
    kirimDataKeGoogle(suhuSesaat, kelembabanSesaat);
  }

  waktuTerakhirKirim = millis();
}

void loop() {
  unsigned long waktuSekarang = millis();

  // ==========================================
  // TUGAS 1: PERBARUI TAMPILAN MONITORING UTAMA (DIPERCEPAT MENJADI SETIAP 200 MS)
  // ==========================================
  if (waktuSekarang - waktuTerakhirOLED >= 200) {  // <--- Ubah dari 1000 menjadi 200
    waktuTerakhirOLED = waktuSekarang;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      suhuSesaat = t;
      kelembabanSesaat = h;
    }

    // LOGIKA EVALUASI RANGE
    if (suhuSesaat < suhuMin || suhuSesaat > suhuMax || kelembabanSesaat < kelembabanMin || kelembabanSesaat > kelembabanMax) {
      isOutOfRange = true;
    } else {
      isOutOfRange = false;
    }

    display.clearDisplay();

    // Header teks (Ukuran 1)
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("MONITORING SENSOR");
    display.println("---------------------");

    // --- BARIS SUHU ---
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("T:");        // Menghilangkan spasi setelah titik dua
    display.setCursor(30, 20);  // Mengunci posisi angka pas di koordinat X=22 (sangat dekat dengan ":")
    display.print(suhuSesaat, 1);

    // Simbol derajat Celcius digeser agar mengikuti angka
    display.setTextSize(1);
    display.print(" o");
    display.setTextSize(2);
    display.print("C");

    // --- BARIS KELEMBABAN ---
    display.setCursor(0, 42);
    display.print("H:");        // Menghilangkan spasi setelah titik dua
    display.setCursor(30, 42);  // Mengunci posisi angka pas di koordinat X=22
    display.print(kelembabanSesaat, 1);
    display.print(" %");

    // Gambar Ikon Pengiriman dan Ikon Warning 'o' di sisi kanan yang sudah lowong
    drawStatusIcon(statusKirimIcon);
    drawWarningIcon(isOutOfRange);

    display.display();
  }

  // ==========================================
  // TUGAS 2: LOGIKA RESET TIMEOUT IKON STATUS
  // ==========================================
  if (statusKirimIcon > 1 && (waktuSekarang - waktuResetIkon >= 5000)) {
    statusKirimIcon = 0;
  }

  // ==========================================
  // TUGAS 3: LOGIKA EVALUASI DELTA & INTERVAL PENGIRIMAN
  // ==========================================
  float selisihSuhu = abs(suhuSesaat - suhuTerkirim);
  float selisihKelembaban = abs(kelembabanSesaat - kelembabanTerkirim);

  bool deltaTerlampaui = (selisihSuhu >= deltaSuhu) || (selisihKelembaban >= deltaKelembaban);
  bool intervalTerlampaui = (waktuSekarang - waktuTerakhirKirim >= intervalData);

  if (deltaTerlampaui || intervalTerlampaui) {
    waktuTerakhirKirim = waktuSekarang;
    kirimDataKeGoogle(suhuSesaat, kelembabanSesaat);
  }

  // ==========================================
  // TUGAS 4: KONTROL ALARM BUZZER (BERDASARKAN STATUS RANGE)
  // ==========================================
  if (isOutOfRange) {
    if (waktuSekarang - waktuTerakhirBuzzer >= 200) {
      waktuTerakhirBuzzer = waktuSekarang;
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}