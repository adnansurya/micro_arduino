#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>          // Library WiFiManager
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// Konfigurasi Telegram (Tetap Hardcode untuk Keamanan)
#define BOTtoken "TOKEN_BOT_ANDA"
#define CHAT_ID "ID_CHAT_ANDA"

// Definisi Pin
const int trigPin = 5;
const int echoPin = 18;
const int rainPin = 19;
const int floatPin = 21;
const int buzzerPin = 22;
const int ledHijau  = 25;
const int ledKuning = 26;
const int ledMerah  = 27;

// Variabel Status
String lastRainStatus = "";
String lastWaterStatus = "";
bool isFloatActive = false;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

void setup() {
  Serial.begin(115200);
  
  // Konfigurasi I/O
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(rainPin, INPUT);
  pinMode(floatPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledHijau, OUTPUT);
  pinMode(ledKuning, OUTPUT);
  pinMode(ledMerah, OUTPUT);

  // --- WiFiManager Section ---
  WiFiManager wm;
  
  // Reset settings (Hanya untuk testing, hapus jika sudah fix)
  // wm.resetSettings(); 

  // Membuat Access Point dengan nama "ESP32_Pendeteksi_Banjir"
  // Jika gagal konek ke WiFi lama, HP akan mendeteksi WiFi ini
  if (!wm.autoConnect("ESP32_Pendeteksi_Banjir")) {
    Serial.println("Gagal Terhubung dan Timeout");
    ESP.restart();
  }

  Serial.println("WiFi Terhubung!");
  client.setInsecure(); // Mengabaikan validasi SSL untuk Telegram
  
  // Notifikasi pertama saat alat menyala
  bot.sendMessage(CHAT_ID, "Sistem Pendeteksi Banjir Online!", "");
}

void loop() {
  // 1. Pembacaan Jarak Ultrasonic
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * 0.034 / 2;

  // 2. Kontrol LED & Buzzer berdasarkan Jarak
  String currentStatus = "";
  if (distance <= 20) {
    currentStatus = "AMAN";
    updateLokalIndikator(HIGH, LOW, LOW, LOW);
  } 
  else if (distance > 20 && distance <= 40) {
    currentStatus = "SIAGA";
    updateLokalIndikator(LOW, HIGH, LOW, LOW);
  } 
  else {
    currentStatus = "BAHAYA";
    updateLokalIndikator(LOW, LOW, HIGH, HIGH);
  }

  // 3. Logika Notifikasi Terpadu (Hanya jika Float Switch Mengapung)
  // Float Switch terhubung ke Ground (LOW saat terangkat/mengapung)
  bool currentFloatState = (digitalRead(floatPin) == LOW); 

  if (currentFloatState) {
    if (currentStatus != lastWaterStatus) {
      String pesan = "⚠️ *PERINGATAN BANJIR!*\n";
      pesan += "Status Float: *MENGAPUNG*\n";
      pesan += "Level Air: " + currentStatus + "\n";
      pesan += "Jarak: " + String(distance) + " cm";
      
      bot.sendMessage(CHAT_ID, pesan, "Markdown");
      lastWaterStatus = currentStatus;
    }
  } else {
    // Reset status jika air turun dan float tidak lagi mengapung
    lastWaterStatus = ""; 
  }

  // 4. Logika Rain Sensor (Mandiri)
  int rainRead = digitalRead(rainPin);
  String currentRainStatus = (rainRead == LOW) ? "HUJAN" : "KERING";
  if (currentRainStatus != lastRainStatus) {
    bot.sendMessage(CHAT_ID, "🌦️ Info Cuaca: *" + currentRainStatus + "*", "Markdown");
    lastRainStatus = currentRainStatus;
  }

  delay(2000);
}

// Fungsi pembantu untuk update LED & Buzzer
void updateLokalIndikator(int h, int k, int m, int b) {
  digitalWrite(ledHijau, h);
  digitalWrite(ledKuning, k);
  digitalWrite(ledMerah, m);
  digitalWrite(buzzerPin, b);
}