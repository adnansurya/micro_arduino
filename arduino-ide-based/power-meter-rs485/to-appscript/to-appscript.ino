#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// Ganti dengan kredensial WiFi Anda
const char* ssid = "DAMAI KEMAKMURAN";
const char* password = "damai321";

// Ganti dengan URL Web App Anda (sudah deploy dan public)
const String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbw5HivqIgz4phUOFdWfPoQDfc_3VPm3T9pqjfDVJxdEw1ODb4Po_AO-k49P9BrhJah4nA/exec";

void setup() {
  Serial.begin(9600);

  // Hubungkan ke WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("✅ Terhubung ke WiFi");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Simulasi data sensor
    float temperature = readTemperature();
    float humidity = readHumidity();

    // Kirim data ke Google Apps Script
    sendToGoogleScript(temperature, humidity);
  } else {
    Serial.println("⚠️ Koneksi WiFi terputus, mencoba reconnect...");
    WiFi.begin(ssid, password);
  }

  delay(30000); // kirim data setiap 30 detik
}

void sendToGoogleScript(float temperature, float humidity) {
  WiFiClientSecure client;
  client.setInsecure();  // disable sertifikat SSL (aman untuk testing)

  HTTPClient http;

  // Mulai koneksi HTTPS
  if (!http.begin(client, GOOGLE_SCRIPT_URL)) {
    Serial.println("❌ Gagal memulai koneksi HTTPS");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  // Buat payload JSON
  DynamicJsonDocument doc(256);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;

  String payload;
  serializeJson(doc, payload);

  Serial.print("📤 Mengirim data: ");
  Serial.println(payload);

  // Kirim request POST
  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("✅ Kode respons HTTP: %d\n", httpCode);
    String response = http.getString();
    Serial.println("📥 Respons: " + response);
  } else {
    Serial.printf("❌ Error pada request: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// Fungsi simulasi pembacaan sensor (ganti dengan sensor sebenarnya)
float readTemperature() {
  // Simulasi suhu antara 25.0 - 30.0
  return 25.0 + (random(0, 100) / 20.0);
}

float readHumidity() {
  // Simulasi kelembaban antara 60.0 - 70.0
  return 60.0 + (random(0, 100) / 10.0);
}
