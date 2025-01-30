#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Warkop SELO";
const char* password = "kopisusu";
const char* serverUrl = "http://192.168.1.96/save-data";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "phase=R&current=2.3&voltage=220&frequency=50&power=500&status=OK";
    int httpResponseCode = http.POST(postData);

    Serial.println(httpResponseCode);
    http.end();
  }
  delay(5000);
}
