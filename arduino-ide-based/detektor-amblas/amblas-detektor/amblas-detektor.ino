#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <vector>

// --- CONFIG ---
const char* BOT_TOKEN = "8720903466:AAFYSSQ62jIluthCvF4BS6kNhaLQSfxzQNI";
const char* SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxfsCkUydDZzfXuNT1_JbqxMttbv9xD0XOqfiyQ8bJ7r3g-LZvf-iDPNe1cx76GZoQ00g/exec";

const int TRIG_KIRI = 12, ECHO_KIRI = 13, TRIG_TENGAH = 14, ECHO_TENGAH = 27, TRIG_KANAN = 26, ECHO_KANAN = 25;
const int LED_HIJAU = 18, LED_KUNING = 19, LED_MERAH = 21, BUZZER = 22;

std::vector<String> receiverList;
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

unsigned long lastSync = 0;
unsigned long lastNotifyTime = 0;
int lastStatus = -1;

void refreshReceivers() {
  HTTPClient http;
  http.begin(String(SCRIPT_URL)); // doGet mengembalikan list receiver
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  if (httpCode > 0) {
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, http.getString());
    receiverList.clear();
    for (JsonVariant v : doc.as<JsonArray>()) {
      receiverList.push_back(v.as<String>());
    }
    Serial.println("List receiver diperbarui.");
  }
  http.end();
}

long readDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long d = pulseIn(echo, HIGH, 25000);
  return (d == 0) ? 999 : (d / 2) / 29.1;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_HIJAU, OUTPUT); pinMode(LED_KUNING, OUTPUT); pinMode(LED_MERAH, OUTPUT); pinMode(BUZZER, OUTPUT);
  pinMode(TRIG_KIRI, OUTPUT); pinMode(ECHO_KIRI, INPUT);
  pinMode(TRIG_TENGAH, OUTPUT); pinMode(ECHO_TENGAH, INPUT);
  pinMode(TRIG_KANAN, OUTPUT); pinMode(ECHO_KANAN, INPUT);
  
  WiFiManager wm;
  wm.autoConnect("Detektor_Amblas_AP");
  
  client.setInsecure();
  refreshReceivers();
}

void loop() {
  // Sinkronisasi daftar penerima dari Spreadsheet tiap 5 menit
  if (millis() - lastSync > 300000) {
    refreshReceivers();
    lastSync = millis();
  }

  long dK = readDistance(TRIG_KIRI, ECHO_KIRI);
  long dT = readDistance(TRIG_TENGAH, ECHO_TENGAH);
  long dKa = readDistance(TRIG_KANAN, ECHO_KANAN);
  long minD = min(dK, min(dT, dKa));

  int currentStatus = 0; // 0:Aman, 1:Waspada, 2:Bahaya
  String msg = "";

  if (minD < 25) { 
    currentStatus = 2; 
    msg = "🚨 EMERGENCY: Truk Amblas! Jarak sasis: " + String(minD) + " cm"; 
  }
  else if (minD <= 40) { 
    currentStatus = 1; 
    msg = "⚠️ WARNING: Jarak sasis rendah (" + String(minD) + " cm)"; 
  }

  // Output Hardware
  if (currentStatus == 2) { 
    digitalWrite(LED_MERAH, 1); digitalWrite(LED_KUNING, 0); digitalWrite(LED_HIJAU, 0); 
    digitalWrite(BUZZER, 1); 
  }
  else if (currentStatus == 1) { 
    digitalWrite(LED_MERAH, 0); digitalWrite(LED_KUNING, 1); digitalWrite(LED_HIJAU, 0); 
    static unsigned long bz = 0;
    if(millis() - bz > 3000){
      for(int i=0; i<3; i++){ digitalWrite(BUZZER, 1); delay(100); digitalWrite(BUZZER, 0); delay(100); }
      bz = millis();
    }
  } else { 
    digitalWrite(LED_MERAH, 0); digitalWrite(LED_KUNING, 0); digitalWrite(LED_HIJAU, 1); 
    digitalWrite(BUZZER, 0); 
  }

  // Pengiriman Notifikasi (Kondisi Amblas atau Interval 15 detik)
  if (currentStatus != 0) {
    if (currentStatus != lastStatus || millis() - lastNotifyTime > 15000) {
      for (String id : receiverList) {
        bot.sendMessage(id, msg, "");
      }
      lastNotifyTime = millis();
      lastStatus = currentStatus;
    }
  } else {
    lastStatus = 0;
  }

  delay(500); 
}