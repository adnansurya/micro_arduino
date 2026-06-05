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

// Definisi Pin LED dan Buzzer
#define LED_HIJAU  14
#define LED_KUNING 27
#define LED_MERAH  26
#define PIN_BUZZER 15  

// Variabel Kalibrasi Fisik Sungai (Dinamis dari Blynk)
int KEDALAMAN_SUNGAI = 400; 
int BATAS_WASPADA    = 200; 
int BATAS_BAHAYA     = 350; 

// Variabel status global untuk kontrol buzzer dan notifikasi
String statusGlobal = "AMAN";
String statusSebelumnya = "AMAN"; 
bool stateBuzzerKedip = false;

BlynkTimer timer;

// Fungsi Callback ketika WiFiManager masuk ke mode Access Point (Config Mode)
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Masuk ke Mode Konfigurasi WiFi");
  Serial.println("SSID AP: " + myWiFiManager->getConfigPortalSSID());
  
  // Tampilkan Nama AP ke LCD agar user tahu harus connect ke hotspot mana
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hubungkan WiFi:");
  lcd.setCursor(0, 1);
  lcd.print(myWiFiManager->getConfigPortalSSID()); // Akan muncul "ESP32-Monitoring"
}

// ================= BLYNK SYNC & WRITE SYNC =================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V2, V3, V4);
}

BLYNK_WRITE(V2) {
  KEDALAMAN_SUNGAI = param.asInt();
  Serial.print("Update Kedalaman Sungai: "); Serial.print(KEDALAMAN_SUNGAI); Serial.println(" cm");
}

BLYNK_WRITE(V3) {
  BATAS_WASPADA = param.asInt();
  Serial.print("Update Batas Waspada: "); Serial.print(BATAS_WASPADA); Serial.println(" cm");
}

BLYNK_WRITE(V4) {
  BATAS_BAHAYA = param.asInt();
  Serial.print("Update Batas Bahaya: "); Serial.print(BATAS_BAHAYA); Serial.println(" cm");
}
// ===========================================================

// Fungsi untuk membaca jarak dari sensor JSN-SR04T
float ambilJarak() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long durasi = pulseIn(ECHO_PIN, HIGH, 30000);
  float jarak = durasi * 0.034 / 2;
  
  if (jarak == 0 || jarak > 400) {
    return -1; 
  }
  return jarak;
}

// Fungsi khusus untuk mengatur bunyi buzzer tanpa delay blocking
void kontrolBuzzer() {
  if (statusGlobal == "AMAN") {
    digitalWrite(PIN_BUZZER, LOW); 
  } 
  else if (statusGlobal == "WASPADA") {
    stateBuzzerKedip = !stateBuzzerKedip;
    digitalWrite(PIN_BUZZER, stateBuzzerKedip);
  } 
  else if (statusGlobal == "BAHAYA") {
    digitalWrite(PIN_BUZZER, HIGH); 
  }
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

  int tinggiAir = KEDALAMAN_SUNGAI - jarakSensor;
  if (tinggiAir < 0) tinggiAir = 0;

  if (tinggiAir < BATAS_WASPADA) {
    statusGlobal = "AMAN";
    digitalWrite(LED_HIJAU, HIGH);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, LOW);
  } 
  else if (tinggiAir >= BATAS_WASPADA && tinggiAir < BATAS_BAHAYA) {
    statusGlobal = "WASPADA";
    digitalWrite(LED_HIJAU, LOW);
    digitalWrite(LED_KUNING, HIGH);
    digitalWrite(LED_MERAH, LOW);
  } 
  else {
    statusGlobal = "BAHAYA";
    digitalWrite(LED_HIJAU, LOW);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, HIGH);
  }

  if (statusGlobal != statusSebelumnya) {
    String pesanNotif = "Peringatan! Status air sungai berubah menjadi: " + statusGlobal + " (" + String(tinggiAir) + " cm)";
    Blynk.logEvent("Status_perubahan", pesanNotif);
    statusSebelumnya = statusGlobal;
  }

  // Tampilkan data ke LCD 16x2
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tg Air: ");
  lcd.print(tinggiAir);
  lcd.print(" cm");
  
  lcd.setCursor(0, 1);
  lcd.print("Status: ");
  lcd.print(statusGlobal);

  // Kirim data monitoring kembali ke Blynk Cloud
  Blynk.virtualWrite(V0, tinggiAir);        
  Blynk.virtualWrite(V1, statusGlobal); 
}

void setup() {
  Serial.begin(115200);
  
  // Atur Mode Pin
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_KUNING, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Mencari WiFi...");

  // Konfigurasi WiFiManager
  WiFiManager wm;
  
  // Set fungsi callback saat masuk mode AP captive portal
  wm.setAPCallback(configModeCallback);
  
  // Mencoba connect ke WiFi lama, jika gagal buka AP "ESP32-Monitoring"
  if (!wm.autoConnect("ESP32-Monitoring")) {
    Serial.println("Gagal terhubung dan timeout konfigurasi");
    lcd.clear();
    lcd.print("Timeout/Gagal!");
    delay(3000);
    ESP.restart();
  }

  // JIKA BERHASIL CONNECT WIFI:
  Serial.println("WiFi Terhubung!");
  
  // Tampilkan nama SSID WiFi yang berhasil terhubung pada LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Terhubung!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.SSID()); // Mengambil nama SSID secara dinamis
  delay(2500);

  // Inisialisasi Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  // Interval utama pembacaan sensor dan buzzer
  timer.setInterval(2000L, sendSensorData);
  timer.setInterval(300L, kontrolBuzzer);
}

void loop() {
  Blynk.run();
  timer.run();
}