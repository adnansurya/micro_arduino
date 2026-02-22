#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// --- KONFIGURASI FISIK ALAT ---
// Ganti angka ini sesuai tinggi pemasangan sensor Anda dalam cm
const float TINGGI_PEMASANGAN_SENSOR = 100.0; 

// --- Konfigurasi Telegram & NTP ---
#define BOTtoken "1389983359:AAHCbBbsuglCOthFan2L02EFKRBJtSCHI38"
#define CHAT_ID "108488036"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", 28800); // WITA

// --- Pin Sensor & Aktuator ---
const int trigPin = 5;
const int echoPin = 18;
const int rainPin = 19;
const int floatPin = 21;
const int buzzerPin = 22;
const int ledHijau  = 25;
const int ledKuning = 26;
const int ledMerah  = 27;

// --- Variabel Global ---
String lastRainStatus = "";
String lastWaterStatus = "";
float jarakBacaan = 0;
float tinggiAir = 0;
String currentStatus = "";
String currentRainStatus = "";
bool isFloatActive = false;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

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
  if (!wm.autoConnect("ESP32_Pendeteksi_Banjir")) {
    ESP.restart();
  }
  
  client.setInsecure();
  timeClient.begin();
}

void loop() {
  // 1. Baca Jarak dari Sensor Ultrasonic
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  jarakBacaan = duration * 0.034 / 2;

  // 2. RUMUS KETINGGIAN AIR
  tinggiAir = TINGGI_PEMASANGAN_SENSOR - jarakBacaan;
  
  // Mencegah nilai minus jika ada error pembacaan
  if (tinggiAir < 0) tinggiAir = 0;

  // 3. Logika Status Berdasarkan Ketinggian Air (Update Sesuai Kebutuhan)
  if (tinggiAir <= 20) {
    currentStatus = "AMAN";
    updateIndikator(HIGH, LOW, LOW, LOW);
  } else if (tinggiAir > 20 && tinggiAir <= 40) {
    currentStatus = "SIAGA";
    updateIndikator(LOW, HIGH, LOW, LOW);
  } else {
    currentStatus = "BAHAYA";
    updateIndikator(LOW, LOW, HIGH, HIGH);
  }

  // 4. Baca Sensor Lain
  int rainRead = digitalRead(rainPin);
  currentRainStatus = (rainRead == LOW) ? "HUJAN" : "KERING";
  isFloatActive = (digitalRead(floatPin) == LOW); 

  tampilSerial();

  // 5. Notifikasi Telegram (Hanya jika Float Mengapung & Status Berubah)
  if (isFloatActive) {
    if (currentStatus != lastWaterStatus) {
      String pesan = "⚠️ *PERINGATAN BANJIR!*\n";
      pesan += "Waktu: `" + timeClient.getFormattedTime() + "`\n";
      pesan += "Status: *" + currentStatus + "*\n";
      pesan += "Tinggi Air: *" + String(tinggiAir) + " cm*\n";
      pesan += "Float Switch: *TERANGKAT*";
      
      bot.sendMessage(CHAT_ID, pesan, "Markdown");
      lastWaterStatus = currentStatus;
    }
  } else {
    lastWaterStatus = ""; 
  }

  // 6. Notifikasi Rain Sensor
  if (currentRainStatus != lastRainStatus) {
    String pesanHujan = "🌦️ *INFO CUACA*\n";
    pesanHujan += "Waktu: `" + timeClient.getFormattedTime() + "`\n";
    pesanHujan += "Kondisi: *" + currentRainStatus + "*";
    
    bot.sendMessage(CHAT_ID, pesanHujan, "Markdown");
    lastRainStatus = currentRainStatus;
  }

  timeClient.update();
  delay(2000); 
}

void updateIndikator(int h, int k, int m, int b) {
  digitalWrite(ledHijau, h);
  digitalWrite(ledKuning, k);
  digitalWrite(ledMerah, m);
  digitalWrite(buzzerPin, b);
}

void tampilSerial() {
  Serial.println("\n--- MONITORING LEVEL AIR ---");
  Serial.print("Waktu       : "); Serial.println(timeClient.getFormattedTime());
  Serial.print("Jarak Sensor: "); Serial.print(jarakBacaan); Serial.println(" cm");
  Serial.print("TINGGI AIR  : "); Serial.print(tinggiAir); Serial.println(" cm");
  Serial.print("Status      : "); Serial.println(currentStatus);
  Serial.print("Float SW    : "); Serial.println(isFloatActive ? "ON" : "OFF");
  Serial.println("----------------------------");
}