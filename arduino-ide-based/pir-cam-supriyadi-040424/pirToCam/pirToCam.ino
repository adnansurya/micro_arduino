
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <HTTPClient.h>

#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);

#define pirPin 13
#define ledPin 2
#define relayPin 5

// Replace with your network credentials
const char* ssid = "MIKRO";
const char* password = "IDEAlist";

// Checks for new messages every 1 second.
int sensorCheckDelay = 50;
unsigned long lastTimeCheckSensor;

int adaGerak = 0;
int lastGerak = 0;

bool relayState = LOW;

String camIP = "http://192.168.188.31";
String camURL = camIP + "/ada_gerakan";

void setup() {
  Serial.begin(115200);  // Open serial monitor at 115200 baud to see ping results.

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  delay(1000);
  digitalWrite(relayPin, LOW);

  pinMode(pirPin, INPUT);

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);


  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    kedipLed(0.2);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot);
  server.on("/buka_pintu", bukaPintu);
  server.on("/kunci_pintu", kunciPintu);
  server.begin();


  kedipLed(2);
}

void loop() {

  if (millis() > lastTimeCheckSensor + sensorCheckDelay) {
    adaGerak = digitalRead(pirPin);
    Serial.print("ADA GERAK : ");
    Serial.print(adaGerak);
    Serial.println();
    if (adaGerak != lastGerak) {  //filter untuk mendeteksi perubahan status
      if (adaGerak) {
        Serial.println("Gerakan Terdeteksi");
        openURL(camURL);
        kedipLed(3);
      }
    }
    lastGerak = adaGerak;
    lastTimeCheckSensor = millis();

  } else {
    server.handleClient();
  }
}


void handleRoot() {
  server.send(200, "text/plain", "hello from esp32!");
}

void bukaPintu() {
  server.send(200, "text/plain", "Membuka Pintu...");
  relayState = HIGH;
  digitalWrite(relayPin, relayState);
}

void kunciPintu() {
  server.send(200, "text/plain", "Mengunci Pintu...");
  relayState = LOW;
  digitalWrite(relayPin, relayState);
}


void kedipLed(float detik) {
  digitalWrite(ledPin, HIGH);
  delay(detik * 1000);
  digitalWrite(ledPin, LOW);
}


void openURL(String urlLink) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(urlLink);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Response: " + payload);
      // Lakukan sesuatu dengan data yang diterima (misalnya, kendalikan perangkat berdasarkan respons)
    } else {
      Serial.println("Error on HTTP request");
    }

    http.end();
    // delay(5000);  // Tunggu 5 detik sebelum mengirim permintaan berikutnya
  }
}
