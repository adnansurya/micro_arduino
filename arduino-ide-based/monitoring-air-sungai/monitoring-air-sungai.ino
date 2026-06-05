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
#define PIN_BUZZER 15  // Pin GPIO 15 dekat port USB

// Variabel Kalibrasi Fisik Sungai (Dinamis dari Blynk)
int KEDALAMAN_SUNGAI = 400; // Nilai default awal (cm)
int BATAS_WASPADA    = 200; // Nilai default awal (cm)
int BATAS_BAHAYA     = 350; // Nilai default awal (cm)

// Variabel status global untuk kontrol buzzer dan notifikasi
String statusGlobal = "AMAN";
String statusSebelumnya = "AMAN"; // Variabel flag untuk mendeteksi perubahan status
bool stateBuzzerKedip = false;

BlynkTimer timer;

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

  // Evaluasi pengkondisian status air
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

  // LOGIKA NOTIFIKASI: Kirim log/push notif hanya saat status mengalami perubahan
  if (statusGlobal != statusSebelumnya) {
    String pesanNotif = "Peringatan! Status air sungai berubah menjadi: " + statusGlobal + " (" + String(tinggiAir) + " cm)";
    Blynk.logEvent("status_perubahan", pesanNotif);
    Serial.println(">>> Notifikasi Dikirim: " + pesanNotif);
    
    // Perbarui status sebelumnya agar tidak mengirim notifikasi berulang
    statusSebelumnya = statusGlobal;
  }

  // Tampilkan data ke Serial Monitor
  Serial.print("Tinggi Air: ");   Serial.print(tinggiAir); Serial.println(" cm");
  Serial.print("Status    : ");   Serial.println(statusGlobal);

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
  lcd.print("Menghubungkan...");

  // Konfigurasi WiFiManager dengan nama Hotspot Baru
  WiFiManager wm;
  if (!wm.autoConnect("ESP32-Monitoring")) {
    Serial.println("Gagal terhubung dan timeout konfigurasi");
    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi Terhubung!");
  lcd.setCursor(0, 1);
  lcd.print("WiFi OK!       ");
  delay(1500);

  // Inisialisasi Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  // Interval utama pembacaan sensor setiap 2 detik
  timer.setInterval(2000L, sendSensorData);

  // Interval pengecekan buzzer setiap 300ms 
  timer.setInterval(300L, kontrolBuzzer);
}

void loop() {
  Blynk.run();
  timer.run();
}