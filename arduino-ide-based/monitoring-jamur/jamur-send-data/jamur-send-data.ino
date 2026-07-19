#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <ArduinoJson.h> // Instal library ArduinoJson dari Library Manager

const char* ssid = "MIKRO1";
const char* password = "1DEAlist1";
const char* scriptUrl = "https://script.google.com/macros/s/AKfycbw8kSNRQZ2OUrIDOxFw0MmzpUyhmF1Su6o4FODbJw6dLm594EV_KaIMlpnzdkNPG9Z-LQ/exec";

#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void loop() {
  delay(5000); // Kirim data tiap 5 detik

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) return;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(scriptUrl);
    http.addHeader("Content-Type", "application/json");

    // Membuat JSON
    StaticJsonDocument<200> doc;
    doc["suhu"] = t;
    doc["kelembaban"] = h;
    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);
    Serial.println("HTTP Response code: " + String(httpResponseCode));
    
    http.end();
  }
}