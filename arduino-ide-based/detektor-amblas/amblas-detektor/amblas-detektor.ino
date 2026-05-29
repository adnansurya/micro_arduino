#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <vector>
#include <TinyGPS++.h> // Tambahan Library GPS

// --- CONFIG ---
const char* SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxfsCkUydDZzfXuNT1_JbqxMttbv9xD0XOqfiyQ8bJ7r3g-LZvf-iDPNe1cx76GZoQ00g/exec";
const char* KODE_UNIT = "LD0492"; 

// Pin JSN-SR04T & Output
const int TRIG_TENGAH = 12, ECHO_TENGAH = 13;
const int TRIG_KANAN = 14, ECHO_KANAN = 27;
const int TRIG_KIRI = 26, ECHO_KIRI = 25;
const int LED_HIJAU = 21, LED_KUNING = 19, LED_MERAH = 18, BUZZER = 22;

// Pin GPS Ublox Neo 6m (HardwareSerial 2)
const int GPS_RX_PIN = 16; // Hubungkan ke TX Modul GPS
const int GPS_TX_PIN = 17; // Hubungkan ke RX Modul GPS (Opsional)

// Objek GPS
TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // Menggunakan UART2 ESP32

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
  
  // Inisialisasi Serial untuk GPS (Baudrate default Neo-6M umumnya 9600)
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
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

  // 3. WAITING FOR GPS LOCK (Menahan sistem sebelum masuk ke loop utama)
  Serial.println("\n=====================================");
  Serial.println("    MENUNGGU KOORDINAT GPS VALID     ");
  Serial.println("=====================================");
  Serial.println("Pastikan antena GPS berada di area terbuka (outdoor)...");
  
  unsigned long lastGpsBlink = 0;
  bool gpsLedState = false;

  while (!gps.location.isValid()) {
    // Selalu baca data stream dari modul GPS di dalam loop penahan ini
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

    // Indikator visual non-blocking: LED Kuning berkedip cepat menandakan "Mencari Satelit"
    if (millis() - lastGpsBlink > 250) {
      gpsLedState = !gpsLedState;
      digitalWrite(LED_KUNING, gpsLedState ? HIGH : LOW);
      
      // Print status satelit ke Serial Monitor agar bisa dipantau
      Serial.printf("[GPS] Menunggu Sinyal... Satelit Terhubung: %d\n", gps.satellites.value());
      lastGpsBlink = millis();
    }
    
    yield(); // Beri waktu jeda background OS ESP32 agar tidak watchdog reset
  }

  // Jika keluar dari loop berarti GPS sudah LOCK
  digitalWrite(LED_KUNING, LOW); // Matikan LED Kuning penanda tadi
  Serial.println("-> GPS Lock Berhasil!");
  Serial.printf("-> Koordinat Awal: %f, %f\n", gps.location.lat(), gps.location.lng());
  
  // Berikan sinyal buzzer/led panjang tanda sistem siap beroperasi penuh
  blinkAll(3, 100); 
  
  Serial.println("\nKIRI\tTENGAH\tKANAN\tSTATUS\t\tLATITUDE\tLONGITUDE");
  Serial.println("-------------------------------------------------------------------------");
}

void loop() {
  // Selalu baca data stream dari GPS secara non-blocking di awal loop
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  long dK = readDistance(TRIG_KIRI, ECHO_KIRI);
  long dT = readDistance(TRIG_TENGAH, ECHO_TENGAH);
  long dKa = readDistance(TRIG_KANAN, ECHO_KANAN);
  
  long minD = dK;
  String sensorName = "Tengah"; 

  if (dK <= dT && dK <= dKa) { minD = dK; sensorName = "Kiri"; }
  else if (dT <= dK && dT <= dKa) { minD = dT; sensorName = "Tengah"; }
  else { minD = dKa; sensorName = "Kanan"; }

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

  // Ambil data koordinat ter-update (Karena di setup sudah dipastikan valid, di sini pasti aman)
  String latStr = String(gps.location.lat(), 6);
  String lngStr = String(gps.location.lng(), 6);

  // Serial Monitor Logger (Setiap 500ms)
  if (millis() - lastSerialUpdate > 500) {
    Serial.printf("%ld\t%ld\t%ld\t%s (%s)\t%s\t%s\n", dK, dT, dKa, statusTxt.c_str(), sensorName.c_str(), latStr.c_str(), lngStr.c_str());
    lastSerialUpdate = millis();
  }

  // --- KONTROL INDIKATOR FISIK ---
  if (currentStatus == 2) { 
    digitalWrite(LED_MERAH, HIGH); digitalWrite(LED_KUNING, LOW); digitalWrite(LED_HIJAU, LOW); 
    digitalWrite(BUZZER, HIGH); 
  }
  else if (currentStatus == 1) { 
    digitalWrite(LED_MERAH, LOW); digitalWrite(LED_KUNING, HIGH); digitalWrite(LED_HIJAU, LOW); 
    
    static unsigned long lastBuzzerAlert = 0;
    if (millis() - lastBuzzerAlert > 3000) {
      for (int i = 0; i < 3; i++) { 
        digitalWrite(BUZZER, HIGH); delay(80); 
        digitalWrite(BUZZER, LOW); delay(80); 
      }
      lastBuzzerAlert = millis();
    }
  } 
  else { 
    digitalWrite(LED_MERAH, LOW); digitalWrite(LED_KUNING, LOW); digitalWrite(LED_HIJAU, HIGH); 
    digitalWrite(BUZZER, LOW); 
  }

  // --- LOGIKA PENGIRIMAN DATA KE GOOGLE APPS SCRIPT ---
  if (currentStatus != 0) {
    if (currentStatus != lastStatus || sensorName != lastCriticalSensor || millis() - lastNotifyTime > 15000) {
      
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      
      http.begin(client, String(SCRIPT_URL));
      http.addHeader("Content-Type", "application/json");
      http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

      StaticJsonDocument<256> dataDoc;
      dataDoc["kode_unit"] = KODE_UNIT;
      dataDoc["sensor_name"] = sensorName;
      dataDoc["jarak"] = minD;
      dataDoc["status"] = statusTxt;
      dataDoc["latitude"] = latStr;   
      dataDoc["longitude"] = lngStr;  

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

  delay(50); 
}