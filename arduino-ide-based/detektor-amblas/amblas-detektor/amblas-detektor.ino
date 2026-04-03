#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <vector>

// --- CONFIG ---
const char* BOT_TOKEN = "ISI_TOKEN_BOT_ANDA";
const char* SCRIPT_URL = "ISI_URL_WEB_APP_APPS_SCRIPT";

// Pin JSN-SR04T & Output
const int TRIG_KIRI = 12, ECHO_KIRI = 13;
const int TRIG_TENGAH = 14, ECHO_TENGAH = 27;
const int TRIG_KANAN = 26, ECHO_KANAN = 25;
const int LED_HIJAU = 18, LED_KUNING = 19, LED_MERAH = 21, BUZZER = 22;

// Variabel Global
std::vector<String> receiverList;
int thresholdAman = 40;   
int thresholdBahaya = 25; 

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

unsigned long lastNotifyTime = 0;
unsigned long lastSerialUpdate = 0;
int lastStatus = -1;

// --- FUNGSI SINKRONISASI (HANYA SAAT STARTUP) ---
void syncDataAtStartup() {
  Serial.println("\n=====================================");
  Serial.println("  SINKRONISASI KONFIGURASI CLOUD    ");
  Serial.println("=====================================");
  
  HTTPClient http;
  http.begin(String(SCRIPT_URL));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  if (httpCode > 0) {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, http.getString());

    if (!error) {
      thresholdAman = doc["thresholds"]["jarak_aman"] | 40;
      thresholdBahaya = doc["thresholds"]["jarak_bahaya"] | 25;
      
      Serial.println("[SETTINGS]");
      Serial.printf("  > Batas Kuning : %d cm\n", thresholdAman);
      Serial.printf("  > Batas Merah  : %d cm\n", thresholdBahaya);

      receiverList.clear();
      Serial.println("[RECEIVERS]");
      JsonArray arr = doc["receivers"].as<JsonArray>();
      for (JsonVariant v : arr) {
        String id = v.as<String>();
        receiverList.push_back(id);
        Serial.printf("  - ID: %s\n", id.c_str());
      }
      Serial.println("\nStatus: Sinkronisasi Berhasil.");
    }
  } else {
    Serial.printf("Gagal Sync. HTTP Error: %d\n", httpCode);
  }
  http.end();
  Serial.println("=====================================\n");
  delay(1000);
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
  if (!wm.autoConnect("Detektor_Amblas_AP")) {
    ESP.restart();
  }
  
  client.setInsecure();
  syncDataAtStartup();
  
  Serial.println("Monitoring Jarak (cm):");
  Serial.println("KIRI\tTENGAH\tKANAN\tSTATUS");
  Serial.println("-------------------------------------");
}

void loop() {
  long dK = readDistance(TRIG_KIRI, ECHO_KIRI);
  long dT = readDistance(TRIG_TENGAH, ECHO_TENGAH);
  long dKa = readDistance(TRIG_KANAN, ECHO_KANAN);
  
  long minD = min(dK, min(dT, dKa));
  int currentStatus = 0; 
  String statusTxt = "AMAN";
  String msg = "";

  // Logika Threshold
  if (minD < thresholdBahaya) { 
    currentStatus = 2; 
    statusTxt = "BAHAYA";
    msg = "🚨 EMERGENCY: Truk Amblas! Jarak: " + String(minD) + " cm"; 
  }
  else if (minD <= thresholdAman) { 
    currentStatus = 1; 
    statusTxt = "WASPADA";
    msg = "⚠️ WARNING: Jarak Rendah (" + String(minD) + " cm)"; 
  }

  // Tampilkan di Serial Monitor setiap 500ms
  if (millis() - lastSerialUpdate > 500) {
    Serial.print(dK); Serial.print("\t");
    Serial.print(dT); Serial.print("\t");
    Serial.print(dKa); Serial.print("\t");
    Serial.println(statusTxt);
    lastSerialUpdate = millis();
  }

  // --- Output Hardware ---
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

  // --- Telegram Broadcast ---
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

  delay(100); 
}