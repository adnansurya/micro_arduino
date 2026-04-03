#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <UniversalTelegramBot.h>

// Pin Definitions
const int TRIG_KIRI = 12, ECHO_KIRI = 13;
const int TRIG_TENGAH = 14, ECHO_TENGAH = 27;
const int TRIG_KANAN = 26, ECHO_KANAN = 25;

const int LED_HIJAU = 18, LED_KUNING = 19, LED_MERAH = 21;
const int BUZZER = 22;

// Variables untuk Telegram & Konfigurasi
char botToken[50] = ""; 
char chatID[20] = "";
bool shouldSaveConfig = false;

WiFiClientSecure client;
UniversalTelegramBot *bot;

unsigned long lastNotifyTime = 0;
const long notifyInterval = 15000; // 15 detik
int lastStatus = 0; // 0: Aman, 1: Waspada, 2: Bahaya

// Fungsi untuk menyimpan konfigurasi ke LittleFS
void saveConfigCallback() {
  shouldSaveConfig = true;
}

long readDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000); // Timeout 30ms
  return (duration / 2) / 29.1;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_HIJAU, OUTPUT); pinMode(LED_KUNING, OUTPUT);
  pinMode(LED_MERAH, OUTPUT); pinMode(BUZZER, OUTPUT);
  pinMode(TRIG_KIRI, OUTPUT); pinMode(ECHO_KIRI, INPUT);
  pinMode(TRIG_TENGAH, OUTPUT); pinMode(ECHO_TENGAH, INPUT);
  pinMode(TRIG_KANAN, OUTPUT); pinMode(ECHO_KANAN, INPUT);

  // Mount LittleFS
  if (LittleFS.begin(true)) {
    if (LittleFS.exists("/config.json")) {
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        StaticJsonDocument<256> json;
        deserializeJson(json, configFile);
        strcpy(botToken, json["botToken"]);
        strcpy(chatID, json["chatID"]);
      }
    }
  }

  // WiFiManager
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);
  WiFiManagerParameter custom_bot_token("token", "Telegram Bot Token", botToken, 50);
  WiFiManagerParameter custom_chat_id("id", "Telegram Chat ID", chatID, 20);
  wm.addParameter(&custom_bot_token);
  wm.addParameter(&custom_chat_id);

  if (!wm.autoConnect("Detektor_Amblas_AP")) {
    ESP.restart();
  }

  strcpy(botToken, custom_bot_token.getValue());
  strcpy(chatID, custom_chat_id.getValue());

  if (shouldSaveConfig) {
    File configFile = LittleFS.open("/config.json", "w");
    StaticJsonDocument<256> json;
    json["botToken"] = botToken;
    json["chatID"] = chatID;
    serializeJson(json, configFile);
    configFile.close();
  }

  client.setInsecure();
  bot = new UniversalTelegramBot(botToken, client);
}

void loop() {
  long dKiri = readDistance(TRIG_KIRI, ECHO_KIRI);
  long dTengah = readDistance(TRIG_TENGAH, ECHO_TENGAH);
  long dKanan = readDistance(TRIG_KANAN, ECHO_KANAN);

  int currentStatus = 0; // Default Aman
  String msg = "";
  long minDistance = min(dKiri, min(dTengah, dKanan));

  // Tentukan Status Terburuk
  if (minDistance < 25 && minDistance > 0) {
    currentStatus = 2; // Bahaya
    msg = "⚠️ BAHAYA! Truk Amblas. Jarak: " + String(minDistance) + " cm";
  } else if (minDistance <= 40 && minDistance >= 25) {
    currentStatus = 1; // Waspada
    msg = "⚠️ WASPADA! Jarak Rendah: " + String(minDistance) + " cm";
  }

  // Kontrol Output & Telegram
  if (currentStatus == 2) { // BAHAYA
    digitalWrite(LED_MERAH, HIGH); digitalWrite(LED_KUNING, LOW); digitalWrite(LED_HIJAU, LOW);
    digitalWrite(BUZZER, HIGH); 
  } else if (currentStatus == 1) { // WASPADA
    digitalWrite(LED_MERAH, LOW); digitalWrite(LED_KUNING, HIGH); digitalWrite(LED_HIJAU, LOW);
    // Buzzer bip 3x setiap 3 detik bisa diatur dengan millis() di sini
    static unsigned long bzMillis = 0;
    if (millis() - bzMillis > 3000) {
       for(int i=0; i<3; i++) { digitalWrite(BUZZER, HIGH); delay(100); digitalWrite(BUZZER, LOW); delay(100); }
       bzMillis = millis();
    }
  } else { // AMAN
    digitalWrite(LED_MERAH, LOW); digitalWrite(LED_KUNING, LOW); digitalWrite(LED_HIJAU, HIGH);
    digitalWrite(BUZZER, LOW);
  }

  // Logika Pengiriman Telegram
  if (currentStatus != lastStatus || ((currentStatus > 0) && (millis() - lastNotifyTime > notifyInterval))) {
    if (currentStatus > 0) {
      bot->sendMessage(chatID, msg, "");
      lastNotifyTime = millis();
    }
    lastStatus = currentStatus;
  }
  
  delay(500); // Stabilitas pembacaan
}