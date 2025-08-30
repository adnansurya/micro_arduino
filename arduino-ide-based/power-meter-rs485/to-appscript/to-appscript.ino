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
    // Baca data listrik 3 fasa
    float voltageP, currentP, powerP;
    float voltageQ, currentQ, powerQ;
    float voltageR, currentR, powerR;
    float totalPower;
    
    readElectricalData(voltageP, currentP, powerP, 
                      voltageQ, currentQ, powerQ, 
                      voltageR, currentR, powerR, 
                      totalPower);

    // Kirim data ke Google Apps Script
    sendToGoogleScript(voltageP, currentP, powerP,
                      voltageQ, currentQ, powerQ,
                      voltageR, currentR, powerR,
                      totalPower);
  } else {
    Serial.println("⚠️ Koneksi WiFi terputus, mencoba reconnect...");
    WiFi.begin(ssid, password);
  }

  delay(30000); // kirim data setiap 30 detik
}

void sendToGoogleScript(float vP, float cP, float pP,
                       float vQ, float cQ, float pQ,
                       float vR, float cR, float pR,
                       float totalP) {
  WiFiClientSecure client;
  client.setInsecure();  // disable sertifikat SSL (aman untuk testing)

  HTTPClient http;

  // Mulai koneksi HTTPS
  if (!http.begin(client, GOOGLE_SCRIPT_URL)) {
    Serial.println("❌ Gagal memulai koneksi HTTPS");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  // Buat payload JSON dengan data listrik 3 fasa
  DynamicJsonDocument doc(1024);
  
  // Data Fasa P
  JsonObject phaseP = doc.createNestedObject("phaseP");
  phaseP["voltage"] = vP;
  phaseP["current"] = cP;
  phaseP["power"] = pP;
  
  // Data Fasa Q
  JsonObject phaseQ = doc.createNestedObject("phaseQ");
  phaseQ["voltage"] = vQ;
  phaseQ["current"] = cQ;
  phaseQ["power"] = pQ;
  
  // Data Fasa R
  JsonObject phaseR = doc.createNestedObject("phaseR");
  phaseR["voltage"] = vR;
  phaseR["current"] = cR;
  phaseR["power"] = pR;
  
  // Total daya
  doc["totalPower"] = totalP;
  doc["timestamp"] = millis(); // Timestamp untuk tracking

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

// Fungsi simulasi pembacaan data listrik 3 fasa
void readElectricalData(float &vP, float &cP, float &pP,
                       float &vQ, float &cQ, float &pQ,
                       float &vR, float &cR, float &pR,
                       float &totalP) {
  
  // Simulasi data Fasa P
  vP = 220.0 + (random(-10, 10) / 10.0); // 220V ±1V
  cP = 5.0 + (random(-20, 20) / 10.0);   // 5A ±2A
  pP = vP * cP;                          // Daya = V × I
  
  // Simulasi data Fasa Q
  vQ = 220.0 + (random(-10, 10) / 10.0);
  cQ = 4.5 + (random(-20, 20) / 10.0);
  pQ = vQ * cQ;
  
  // Simulasi data Fasa R
  vR = 220.0 + (random(-10, 10) / 10.0);
  cR = 5.5 + (random(-20, 20) / 10.0);
  pR = vR * cR;
  
  // Total daya
  totalP = pP + pQ + pR;
  
  // Tampilkan data di Serial Monitor untuk debugging
  Serial.println("📊 Data Listrik 3 Fasa:");
  Serial.printf("Fasa P: %.1fV, %.1fA, %.1fW\n", vP, cP, pP);
  Serial.printf("Fasa Q: %.1fV, %.1fA, %.1fW\n", vQ, cQ, pQ);
  Serial.printf("Fasa R: %.1fV, %.1fA, %.1fW\n", vR, cR, pR);
  Serial.printf("Total Daya: %.1fW\n", totalP);
}