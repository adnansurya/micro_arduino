#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Konfigurasi Telegram
#define BOTtoken "TOKEN_BOT_ANDA"
#define CHAT_ID "ID_CHAT_ANDA"

// Konfigurasi NTP (Waktu)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", 28800); // 25200 detik = GMT+7 (WIB)

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

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Fungsi untuk mendapatkan waktu format String
String getFormattedTime() {
  timeClient.update();
  return timeClient.getFormattedTime();
}

void setup() {
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(rainPin, INPUT);
  pinMode(floatPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledHijau, OUTPUT);
  pinMode(ledKuning, OUTPUT);
  pinMode(ledMerah, OUTPUT);

  WiFiManager wm;
  // wm.resetSettings(); // Buka komentar ini jika ingin menghapus memori WiFi

  if (!wm.autoConnect("ESP32_Pendeteksi_Banjir")) {
    Serial.println("Gagal terhubung ke WiFi");
    ESP.restart();
  }

  Serial.println("WiFi Terhubung!");
  client.setInsecure();
  
  timeClient.begin();
  
  bot.sendMessage(CHAT_ID, "🚀 Sistem Aktif pada: " + getFormattedTime(), "");
}

void loop() {
  // 1. Pembacaan Jarak Ultrasonic
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * 0.034 / 2;

  // 2. Kontrol LED & Buzzer
  String currentStatus = "";
  if (distance <= 20) {
    currentStatus = "AMAN";
    updateIndikator(HIGH, LOW, LOW, LOW);
  } 
  else if (distance > 20 && distance <= 40) {
    currentStatus = "SIAGA";
    updateIndikator(LOW, HIGH, LOW, LOW);
  } 
  else {
    currentStatus = "BAHAYA";
    updateIndikator(LOW, LOW, HIGH, HIGH);
  }

  // 3. Logika Notifikasi Terpadu (Hanya jika Float Switch Mengapung)
  bool currentFloatState = (digitalRead(floatPin) == LOW); 

  if (currentFloatState) {
    if (currentStatus != lastWaterStatus) {
      String pesan = "⚠️ *PERINGATAN BANJIR!*\n";
      pesan += "Waktu: `" + getFormattedTime() + "`\n";
      pesan += "Status Float: *MENGAPUNG*\n";
      pesan += "Level Air: *" + currentStatus + "*\n";
      pesan += "Jarak: " + String(distance) + " cm";
      
      bot.sendMessage(CHAT_ID, pesan, "Markdown");
      lastWaterStatus = currentStatus;
    }
  } else {
    lastWaterStatus = ""; 
  }

  // 4. Logika Rain Sensor
  int rainRead = digitalRead(rainPin);
  String currentRainStatus = (rainRead == LOW) ? "HUJAN" : "KERING";
  if (currentRainStatus != lastRainStatus) {
    String pesanHujan = "🌦️ *INFO CUACA*\n";
    pesanHujan += "Waktu: `" + getFormattedTime() + "`\n";
    pesanHujan += "Kondisi: *" + currentRainStatus + "*";
    
    bot.sendMessage(CHAT_ID, pesanHujan, "Markdown");
    lastRainStatus = currentRainStatus;
  }

  delay(2000);
}

void updateIndikator(int h, int k, int m, int b) {
  digitalWrite(ledHijau, h);
  digitalWrite(ledKuning, k);
  digitalWrite(ledMerah, m);
  digitalWrite(buzzerPin, b);
}