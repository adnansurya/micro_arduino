#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// Konfigurasi Wi-Fi dan Telegram
const char* ssid = "NAMA_WIFI";
const char* password = "PASSWORD_WIFI";
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
bool lastFloatState = false;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(rainPin, INPUT);
  pinMode(floatPin, INPUT_PULLUP);
  pinMode(ledHijau, OUTPUT);
  pinMode(ledKuning, OUTPUT);
  pinMode(ledMerah, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  
  // Koneksi WiFi
  WiFi.begin(ssid, password);
  client.setInsecure(); // Untuk memudahkan koneksi HTTPS Telegram
}

void loop() {
  long duration;
  float distance;
  
  // Baca Ultrasonic
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  // Logika Status dan Kontrol LED/Buzzer
  String currentWaterStatus = "";
  
  if (distance <= 20) {
    currentWaterStatus = "AMAN";
    digitalWrite(ledHijau, HIGH);
    digitalWrite(ledKuning, LOW);
    digitalWrite(ledMerah, LOW);
    digitalWrite(buzzerPin, LOW);
  } 
  else if (distance > 20 && distance <= 40) {
    currentWaterStatus = "SIAGA";
    digitalWrite(ledHijau, LOW);
    digitalWrite(ledKuning, HIGH);
    digitalWrite(ledMerah, LOW);
    digitalWrite(buzzerPin, LOW);
  } 
  else {
    currentWaterStatus = "BAHAYA";
    digitalWrite(ledHijau, LOW);
    digitalWrite(ledKuning, LOW);
    digitalWrite(ledMerah, HIGH);
    digitalWrite(buzzerPin, HIGH); // Buzzer aktif hanya di kondisi Bahaya
  }

  // Kirim Notif Telegram jika status berubah
  if (currentWaterStatus != lastWaterStatus) {
    bot.sendMessage(CHAT_ID, "⚠️ Update Level Air: " + currentWaterStatus + " (" + String(distance) + "cm)", "");
    lastWaterStatus = currentWaterStatus;
  }

  // Cek Rain Sensor
  int rainValue = digitalRead(rainPin);
  String currentRainStatus = (rainValue == LOW) ? "HUJAN" : "KERING";
  if (currentRainStatus != lastRainStatus) {
    bot.sendMessage(CHAT_ID, "Info Cuaca: " + currentRainStatus, "");
    lastRainStatus = currentRainStatus;
  }

  // Cek Float Switch
  bool currentFloatState = digitalRead(floatPin);
  if (currentFloatState == LOW && currentFloatState != lastFloatState) { // LOW biasanya berarti terangkat/mengapung
    bot.sendMessage(CHAT_ID, "PERINGATAN: Float Switch Terdeteksi Mengapung!", "");
  }
  lastFloatState = currentFloatState;

  delay(2000); // Jeda pembacaan
}