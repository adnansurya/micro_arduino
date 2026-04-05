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

// Pin JSN-SR04T & Output
const int TRIG_TENGAH = 12, ECHO_TENGAH = 13;
const int TRIG_KANAN = 14, ECHO_KANAN = 27;
const int TRIG_KIRI = 26, ECHO_KIRI = 25;
const int LED_HIJAU = 21, LED_KUNING = 19, LED_MERAH = 18, BUZZER = 22;

// Variabel Global
std::vector<String> receiverList;
int thresholdAman = 40;   
int thresholdBahaya = 25; 

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

unsigned long lastNotifyTime = 0;
unsigned long lastSerialUpdate = 0;
int lastStatus = -1;
String lastCriticalSensor = ""; 

// Fungsi pembantu untuk mengedipkan semua indikator
void blinkAll(int repeat, int duration) {
  for (int i = 0; i < repeat; i++) {
    digitalWrite(LED_HIJAU, HIGH);
    digitalWrite(LED_KUNING, HIGH);
    digitalWrite(LED_MERAH, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(duration);
    digitalWrite(LED_HIJAU, LOW);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, LOW);
    digitalWrite(BUZZER, LOW);
    if (i < repeat - 1) delay(duration);
  }
}

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
      
      receiverList.clear();
      JsonArray arr = doc["receivers"].as<JsonArray>();
      for (JsonVariant v : arr) {
        receiverList.push_back(v.as<String>());
      }
      Serial.println("Status: Sinkronisasi Berhasil.");
    }
  } else {
    Serial.printf("Gagal Sync. HTTP Error: %d\n", httpCode);
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

  // 1. Kedip sekali saat awal alat dinyalakan (Power ON)
  blinkAll(1, 500);

  WiFiManager wm;
  wm.setConfigPortalTimeout(60); // Optional: timeout jika tidak ada input di portal AP
  if (!wm.autoConnect("Detektor_Amblas_AP")) { 
    Serial.println("Gagal tersambung WiFi, Restart...");
    delay(3000);
    ESP.restart(); 
  }
  
  // 2. Kedip dua kali setelah tersambung WiFi
  blinkAll(2, 200);
  
  client.setInsecure();
  syncDataAtStartup();

  // 3. Kirim pesan inisiasi ke semua receiver di list
  String initMsg = "✅ Sistem Detektor Amblas Aktif\nWiFi: " + WiFi.SSID() + "\nIP: " + WiFi.localIP().toString();
  for (String id : receiverList) {
    bot.sendMessage(id, initMsg, "");
  }
  
  Serial.println("KIRI\tTENGAH\tKANAN\tSTATUS");
  Serial.println("-------------------------------------");
}

void loop() {
  long dK = readDistance(TRIG_KIRI, ECHO_KIRI);
  long dT = readDistance(TRIG_TENGAH, ECHO_TENGAH);
  long dKa = readDistance(TRIG_KANAN, ECHO_KANAN);
  
  long minD = dK;
  String sensorName = "Kiri";

  if (dT < minD) { minD = dT; sensorName = "Tengah"; }
  if (dKa < minD) { minD = dKa; sensorName = "Kanan"; }

  int currentStatus = 0; 
  String statusTxt = "AMAN";
  String msg = "";

  if (minD < thresholdBahaya) { 
    currentStatus = 2; 
    statusTxt = "BAHAYA";
    msg = "🚨 EMERGENCY: Chasis bagian " + sensorName + " AMBLAS! (Jarak: " + String(minD) + " cm)"; 
  }
  else if (minD <= thresholdAman) { 
    currentStatus = 1; 
    statusTxt = "WASPADA";
    msg = "⚠️ WARNING: Chasis bagian " + sensorName + " rendah. (Jarak: " + String(minD) + " cm)"; 
  }

  if (millis() - lastSerialUpdate > 500) {
    Serial.printf("%ld\t%ld\t%ld\t%s (%s)\n", dK, dT, dKa, statusTxt.c_str(), sensorName.c_str());
    lastSerialUpdate = millis();
  }

  if (currentStatus == 2) { 
    digitalWrite(LED_MERAH, 1); digitalWrite(LED_KUNING, 0); digitalWrite(LED_HIJAU, 0); digitalWrite(BUZZER, 1); 
  }
  else if (currentStatus == 1) { 
    digitalWrite(LED_MERAH, 0); digitalWrite(LED_KUNING, 1); digitalWrite(LED_HIJAU, 0); 
    static unsigned long bz = 0;
    if(millis() - bz > 3000){
      for(int i=0; i<3; i++){ digitalWrite(BUZZER, 1); delay(100); digitalWrite(BUZZER, 0); delay(100); }
      bz = millis();
    }
  } else { 
    digitalWrite(LED_MERAH, 0); digitalWrite(LED_KUNING, 0); digitalWrite(LED_HIJAU, 1); digitalWrite(BUZZER, 0); 
  }

  if (currentStatus != 0) {
    if (currentStatus != lastStatus || sensorName != lastCriticalSensor || millis() - lastNotifyTime > 15000) {
      for (String id : receiverList) {
        bot.sendMessage(id, msg, "");
      }
      lastNotifyTime = millis();
      lastStatus = currentStatus;
      lastCriticalSensor = sensorName;
    }
  } else {
    lastStatus = 0;
    lastCriticalSensor = "";
  }

  delay(100); 
}