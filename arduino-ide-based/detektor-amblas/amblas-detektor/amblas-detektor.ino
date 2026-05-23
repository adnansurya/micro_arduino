#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <vector>

// --- CONFIG ---
const char* SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxfsCkUydDZzfXuNT1_JbqxMttbv9xD0XOqfiyQ8bJ7r3g-LZvf-iDPNe1cx76GZoQ00g/exec";
const char* KODE_UNIT = "LD0492"; 

// Pin JSN-SR04T & Output
const int TRIG_TENGAH = 12, ECHO_TENGAH = 13;
const int TRIG_KANAN = 14, ECHO_KANAN = 27;
const int TRIG_KIRI = 26, ECHO_KIRI = 25;
const int LED_HIJAU = 21, LED_KUNING = 19, LED_MERAH = 18, BUZZER = 22;

// Variabel Global
int thresholdAman = 40;   
int thresholdBahaya = 25; 

unsigned long lastNotifyTime = 0;
unsigned long lastSerialUpdate = 0;
int lastStatus = -1;
String lastCriticalSensor = ""; 

// Fungsi pembantu untuk mengedipkan semua indikator (Gunakan hanya di setup)
void blinkAll(int repeat, int duration) {
  for (int i = 0; i < repeat; i++) {
    digitalWrite(LED_HIJAU, HIGH); digitalWrite(LED_KUNING, HIGH);
    digitalWrite(LED_MERAH, HIGH); digitalWrite(BUZZER, HIGH);
    delay(duration);
    digitalWrite(LED_HIJAU, LOW); digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, LOW); digitalWrite(BUZZER, LOW);
    if (i < repeat - 1) delay(duration);
  }
}

// Mengambil thresholds konfigurasi dari Cloud saat startup
void syncDataAtStartup() {
  Serial.println("\n=====================================");
  Serial.println("   SINKRONISASI KONFIGURASI CLOUD    ");
  Serial.println("=====================================");
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  http.begin(client, String(SCRIPT_URL));
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  if (httpCode > 0) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, http.getString());

    if (!error) {
      thresholdAman = doc["thresholds"]["jarak_aman"] | 40;
      thresholdBahaya = doc["thresholds"]["jarak_bahaya"] | 25;
      Serial.printf("Sync Berhasil. Jarak Aman: %d cm, Bahaya: %d cm\n", thresholdAman, thresholdBahaya);
    } else {
      Serial.println("Gagal parsing JSON dari Cloud, menggunakan default.");
    }
  } else {
    Serial.printf("Gagal Sync. HTTP Error: %d\n", httpCode);
  }
  http.end();
}

// Fungsi membaca sensor ultrasonik
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

  // 1. Indikator Power On
  blinkAll(1, 500);

  WiFiManager wm;
  wm.setConfigPortalTimeout(60); 
  if (!wm.autoConnect("Detektor_Amblas_AP")) { 
    Serial.println("Gagal tersambung WiFi, Restart...");
    delay(3000);
    ESP.restart(); 
  }
  
  // 2. Indikator WiFi Connected
  blinkAll(2, 200);
  
  // Ambil thresholds terbaru dari Google Sheet
  syncDataAtStartup();
  
  Serial.println("\nKIRI\tTENGAH\tKANAN\tSTATUS");
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

  if (minD < thresholdBahaya) { 
    currentStatus = 2; 
    statusTxt = "BAHAYA";
  }
  else if (minD <= thresholdAman) { 
    currentStatus = 1; 
    statusTxt = "WASPADA";
  }

  // Serial Monitor Logger (Setiap 500ms)
  if (millis() - lastSerialUpdate > 500) {
    Serial.printf("%ld\t%ld\t%ld\t%s (%s)\n", dK, dT, dKa, statusTxt.c_str(), sensorName.c_str());
    lastSerialUpdate = millis();
  }

  // --- KONTROL INDIKATOR FISIK ---
  if (currentStatus == 2) { // BAHAYA: LED Merah & Buzzer ON terus menerus
    digitalWrite(LED_MERAH, HIGH); digitalWrite(LED_KUNING, LOW); digitalWrite(LED_HIJAU, LOW); 
    digitalWrite(BUZZER, HIGH); 
  }
  else if (currentStatus == 1) { // WASPADA: LED Kuning ON, Buzzer berkedip tiap 3 detik (Non-Blocking)
    digitalWrite(LED_MERAH, LOW); digitalWrite(LED_KUNING, HIGH); digitalWrite(LED_HIJAU, LOW); 
    
    static unsigned long lastBuzzerAlert = 0;
    if (millis() - lastBuzzerAlert > 3000) {
      // Pola kedip buzzer tanpa menghentikan pembacaan sensor (delay mikro)
      for (int i = 0; i < 3; i++) { 
        digitalWrite(BUZZER, HIGH); delay(80); 
        digitalWrite(BUZZER, LOW); delay(80); 
      }
      lastBuzzerAlert = millis();
    }
  } 
  else { // AMAN: LED Hijau ON, perangkat output lainnya OFF
    digitalWrite(LED_MERAH, LOW); digitalWrite(LED_KUNING, LOW); digitalWrite(LED_HIJAU, HIGH); 
    digitalWrite(BUZZER, LOW); 
  }

  // --- LOGIKA PENGIRIMAN DATA KE GOOGLE APPS SCRIPT ---
  if (currentStatus != 0) {
    // Kirim jika status berubah, sensor kritis berpindah, atau sudah melewati 15 detik dari notifikasi terakhir
    if (currentStatus != lastStatus || sensorName != lastCriticalSensor || millis() - lastNotifyTime > 15000) {
      
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      
      http.begin(client, String(SCRIPT_URL));
      http.addHeader("Content-Type", "application/json");
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

      // Siapkan JSON Payload data rekap
      StaticJsonDocument<256> dataDoc;
      dataDoc["kode_unit"] = KODE_UNIT;
      dataDoc["sensor_name"] = sensorName;
      dataDoc["jarak"] = minD;
      dataDoc["status"] = statusTxt;
      dataDoc["latitude"] = "";   // Placeholder GPS (Mendukung integrasi masa depan)
      dataDoc["longitude"] = "";  // Placeholder GPS (Mendukung integrasi masa depan)
      dataDoc["url"] = "";        // Placeholder URL Foto (Mendukung integrasi masa depan)

      String requestBody;
      serializeJson(dataDoc, requestBody);

      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode > 0) {
        Serial.printf("-> Cloud Sync Berhasil (HTTP %d)\n", httpResponseCode);
      } else {
        Serial.printf("-> Cloud Sync Gagal (%s)\n", http.errorToString(httpResponseCode).c_str());
      }
      http.end();

      lastNotifyTime = millis();
      lastStatus = currentStatus;
      lastCriticalSensor = sensorName;
    }
  } else {
    lastStatus = 0;
    lastCriticalSensor = "";
  }

  delay(50); // Delay kecil agar loop berjalan stabil (~20Hz)
}