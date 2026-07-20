#include <Arduino.h>
#include <WiFiManager.h>
#include <ElegantOTA.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <coap-simple.h>

// Konfigurasi Pin
#define DHTPIN 1
#define DHTTYPE DHT22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Objek
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);
WiFiUDP udp;
Coap coap(udp);

float suhu = 0, kelembaban = 0;

// Callback saat masuk mode AP
void configModeCallback(WiFiManager *myWiFiManager) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Mode Konfigurasi");
  display.println("AP: ESP32-Monitoring");
  display.display();
}

// Callback CoAP
void callback_suhu(CoapPacket &packet, IPAddress ip, int port) {
  String response = String(suhu) + "," + String(kelembaban);
  coap.sendResponse(ip, port, packet.messageid, response.c_str(), response.length());
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);

  // WiFiManager Setup
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  wm.setConfigPortalTimeout(30);

  if (!wm.autoConnect("ESP32-Monitoring")) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Timeout/Gagal!");
    display.display();
    delay(3000);
    ESP.restart();
  }
  
  // OTA Setup
  server.on("/", []() { server.send(200, "text/plain", "LOLIN S2 Ready"); });
  ElegantOTA.begin(&server);
  server.begin();

  // CoAP Setup
  coap.server(callback_suhu, "data");
  coap.start();
}

void loop() {
  suhu = dht.readTemperature();
  kelembaban = dht.readHumidity();
  
  display.clearDisplay();
  display.setCursor(0, 0);
  
  // Tampilkan data sensor
  display.print("Suhu: "); display.print(suhu); display.println(" C");
  display.print("Hum: "); display.print(kelembaban); display.println(" %");
  
  // Tampilkan IP Address
  display.println("----------------");
  display.print("IP: ");
  display.println(WiFi.localIP());
  
  display.display();

  server.handleClient();
  coap.loop();
  delay(2000);
}