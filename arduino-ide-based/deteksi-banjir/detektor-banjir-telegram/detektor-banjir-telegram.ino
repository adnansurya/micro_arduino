#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// --- KONFIGURASI ---
float TINGGI_DASAR_KALIBRASI = 0; // Akan diisi otomatis saat setup
const int RAIN_THRESHOLD = 2000;   // Sesuaikan nilai ini (0-4095). Semakin kecil angkanya, semakin sensitif terhadap air.

#define BOTtoken "1389983359:AAHCbBbsuglCOthFan2L02EFKRBJtSCHI38"
#define CHAT_ID "108488036"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", 28800); // WITA

// --- Pin Sensor & Aktuator ---
const int trigPin = 5;
const int echoPin = 18;
const int rainAnalogPin = 34; // Gunakan pin ADC (contoh GPIO 34)
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

// Fungsi untuk membaca jarak ultrasonic
float ambilJarak() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float hasil = duration * 0.034 / 2;
  return hasil;
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
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
  timeClient.update();

  // --- PROSES KALIBRASI ---
  Serial.println("Sedang Kalibrasi Jarak Dasar...");
  delay(2000); // Tunggu sensor stabil
  
  // Ambil rata-rata 5 kali pembacaan agar akurat
  float totalJarak = 0;
  for(int i=0; i<5; i++) {
    totalJarak += ambilJarak();
    delay(200);
  }
  TINGGI_DASAR_KALIBRASI = totalJarak / 5;

  Serial.print("Kalibrasi Selesai. Tinggi Sensor: ");
  Serial.println(TINGGI_DASAR_KALIBRASI);

  // Kirim Notifikasi Kalibrasi ke Telegram
  String pesanKalibrasi = "✅ *Sistem Online & Terkalibrasi*\n";
  pesanKalibrasi += "Waktu: `" + timeClient.getFormattedTime() + "`\n";
  pesanKalibrasi += "Tinggi Pemasangan: *" + String(TINGGI_DASAR_KALIBRASI) + " cm*";
  bot.sendMessage(CHAT_ID, pesanKalibrasi, "Markdown");
}

void loop() {
  // 1. Baca Ketinggian Air
  jarakBacaan = ambilJarak();
  tinggiAir = TINGGI_DASAR_KALIBRASI - jarakBacaan;
  if (tinggiAir < 0) tinggiAir = 0;

  // 2. Logika Status Ketinggian
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

  // 3. Baca Rain Sensor (Analog)
  int rainValue = analogRead(rainAnalogPin);
  // Pada Rain Sensor, semakin basah nilainya semakin KECIL
  currentRainStatus = (rainValue < RAIN_THRESHOLD) ? "HUJAN" : "KERING";

  // 4. Baca Float Switch
  isFloatActive = (digitalRead(floatPin) == LOW); 

  tampilSerial(rainValue);

  // 5. Notifikasi Telegram Banjir
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

  // 6. Notifikasi Telegram Cuaca
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

void tampilSerial(int rv) {
  Serial.println("\n--- MONITORING LEVEL AIR ---");
  Serial.print("Waktu       : "); Serial.println(timeClient.getFormattedTime());
  Serial.print("Tinggi Ref  : "); Serial.print(TINGGI_DASAR_KALIBRASI); Serial.println(" cm");
  Serial.print("Tinggi Air  : "); Serial.print(tinggiAir); Serial.println(" cm");
  Serial.print("Analog Rain : "); Serial.print(rv); Serial.print(" ("); Serial.print(currentRainStatus); Serial.println(")");
  Serial.print("Float SW    : "); Serial.println(isFloatActive ? "ON" : "OFF");
  Serial.println("----------------------------");
}