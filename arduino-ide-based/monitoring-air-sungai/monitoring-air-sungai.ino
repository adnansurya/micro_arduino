#define BLYNK_TEMPLATE_ID "TMPL28Fp5U_un"
#define BLYNK_TEMPLATE_NAME "Monitoring Air Sungai"
#define BLYNK_AUTH_TOKEN "_3kOCp683mkmRxtkXmzg3s6p5NoHmHe3"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi LCD I2C (Alamat 0x27 atau 0x3F, ukuran 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Definisi Pin Sensor JSN-SR04T
#define TRIG_PIN 12
#define ECHO_PIN 13

// Definisi Pin LED
#define LED_HIJAU  14
#define LED_KUNING 27
#define LED_MERAH  26

// Kalibrasi Fisik Sungai (Satuan: centimeter)
const int TINGGI_MAKSIMAL_SUNGAI = 500; // Jarak sensor ke dasar sungai kosong
const int BATAS_WASPADA = 50;          // Air mencapai tinggi 100 cm
const int BATAS_BAHAYA = 70;           // Air mencapai tinggi 150 cm

BlynkTimer timer;

// Fungsi untuk membaca jarak dari sensor JSN-SR04T
float ambilJarak() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long durasi = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout 30ms jika tidak ada pantulan
  
  // Menghitung jarak dalam cm
  float jarak = durasi * 0.034 / 2;
  
  // JSN-SR04T memiliki blind spot ~20cm. Jika jarak 0 atau di luar jangkauan, anggap error
  if (jarak == 0 || jarak > 400) {
    return -1; 
  }
  return jarak;
}

// Fungsi utama monitoring yang dieksekusi berkala oleh BlynkTimer
void sendSensorData() {
  float jarakSensor = ambilJarak();
  
  if (jarakSensor == -1) {
    Serial.println("Gagal membaca sensor atau di luar jangkauan!");
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error   ");
    return;
  }

  // Hitung tinggi air aktual
  int tinggiAir = TINGGI_MAKSIMAL_SUNGAI - jarakSensor;
  if (tinggiAir < 0) tinggiAir = 0; // Mengatasi pembacaan fluktuatif di dasar

  String statusKetinggian = "";
  
  // Logika Status, LED, dan Pengkondisian
  if (tinggiAir < BATAS_WASPADA) {
    statusKetinggian = "AMAN";
    digitalWrite(LED_HIJAU, HIGH);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, LOW);
  } 
  else if (tinggiAir >= BATAS_WASPADA && tinggiAir < BATAS_BAHAYA) {
    statusKetinggian = "WASPADA";
    digitalWrite(LED_HIJAU, LOW);
    digitalWrite(LED_KUNING, HIGH);
    digitalWrite(LED_MERAH, LOW);
  } 
  else {
    statusKetinggian = "BAHAYA";
    digitalWrite(LED_HIJAU, LOW);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, HIGH);
  }

  // Tampilkan data ke Serial Monitor
  Serial.print("Tinggi Air: ");   Serial.print(tinggiAir); Serial.println(" cm");
  Serial.print("Status    : ");   Serial.println(statusKetinggian);

  // Tampilkan data ke LCD 16x2
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tg Air: ");
  lcd.print(tinggiAir);
  lcd.print(" cm");
  
  lcd.setCursor(0, 1);
  lcd.print("Status: ");
  lcd.print(statusKetinggian);

  // Kirim data ke Blynk Cloud
  Blynk.virtualWrite(V0, tinggiAir);        // V1 untuk Gauge / Value Display (Integer)
  Blynk.virtualWrite(V1, statusKetinggian); // V2 untuk Label Value Display (String)
}

void setup() {
  Serial.begin(115200);
  
  // Atur Mode Pin
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_KUNING, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Menghubungkan...");

  // Konfigurasi WiFiManager
  WiFiManager wm;
  // Jika gagal terhubung ke WiFi lama, ESP32 akan membuat Access Point bernama "ESP32-Sistem-Banjir"
  if (!wm.autoConnect("ESP32-Sistem-Banjir")) {
    Serial.println("Gagal terhubung dan timeout konfigurasi");
    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi Terhubung!");
  lcd.setCursor(0, 1);
  lcd.print("WiFi OK!       ");
  delay(1500);

  // Inisialisasi Blynk dengan koneksi WiFiManager yang sudah ada
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  // Mengatur interval pembacaan sensor setiap 2 detik (2000ms)
  timer.setInterval(2000L, sendSensorData);
}

void loop() {
  Blynk.run();
  timer.run();
}