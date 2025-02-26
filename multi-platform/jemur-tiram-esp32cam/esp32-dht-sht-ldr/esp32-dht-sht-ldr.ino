#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <WiFiUDP.h>
#include <DHT.h>
#include <UniversalTelegramBot.h>

// Library dan fungsi Firebase
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Sensor dan koneksi Wi-Fi
#define WIFI_SSID "Can jie"
#define WIFI_PASSWORD "qwerty015"
#define API_KEY "AIzaSyAEJvUK8RGqR-tI6QyRb2bUMF6Y2mUj3D4"
#define DATABASE_URL "https://thelast-e2d6d-default-rtdb.firebaseio.com/"

// Ganti dengan token bot Telegram Anda
// String BOTtoken = "7714606423:AAE-sFrXKqkawnGxAN3MgWwIyxQgnxW5yHY";
String BOTtoken = "1837465469:AAHGQzX5EzMhAGCKkHS8IiBvEJJ5t1e6O8c";
String CHAT_ID = "108488036";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

Adafruit_SHT31 sht31 = Adafruit_SHT31();
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define LDRPIN 34
int ldrValue = 0;

float kelembapan = 0;
float temperatureFloat;

WiFiUDP udp;
unsigned int localPort = 9999;
char packetBuffer[255];
String textData = "";

// Definisi pin untuk DHT22
#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  if (!sht31.begin(0x44)) {  // Alamat I2C dari SHT31
    Serial.println("Tidak bisa menemukan sensor SHT31");
    while (1) delay(1);
  }

  pinMode(LDRPIN, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(localPort);
  Serial.printf("UDP server : %s:%i \n", WiFi.localIP().toString().c_str(), localPort);
  client.setInsecure();  // Disable SSL certificate validation

  bot.sendMessage(CHAT_ID, "ESP32 IP: " + WiFi.localIP().toString(), "");

  // Konfigurasi Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signed up successfully");
  } else {
    Serial.printf("Firebase sign up failed: %s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  bacaLDR();
  bacaSHT();
  bacaSuhu();
  waitDataCommand();
  delay(10);
}

void bacaLDR() {
  ldrValue = analogRead(LDRPIN);
  Serial.print("Nilai LDR: ");
  Serial.println(ldrValue);
}

void bacaSHT() {
  kelembapan = sht31.readHumidity();
  if (!isnan(kelembapan)) {
    Serial.print("Kelembapan: ");
    Serial.print(kelembapan);
    Serial.println(" %");
  } else {
    Serial.println("Gagal membaca kelembapan!");
  }
}

void bacaSuhu() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    temperatureFloat = t;
    Serial.println("Temperature: " + String(t) + "°C");
  }
}

void kirimData() {

  if (Firebase.RTDB.setFloat(&fbdo, "sensor/kelembapan", kelembapan)) {
    Serial.println("Humidity value sent to Firebase successfully");
  } else {
    Serial.printf("Failed to send humidity value: %s\n", fbdo.errorReason().c_str());
  }

  if (Firebase.RTDB.setFloat(&fbdo, "sensor/suhu", temperatureFloat)) {
    Serial.println("Temperature value sent to Firebase successfully");
  } else {
    Serial.printf("Failed to send temperature value: %s\n", fbdo.errorReason().c_str());
  }

  if (Firebase.RTDB.setInt(&fbdo, "sensor/cahaya", ldrValue)) {
    Serial.println("LDR value sent to Firebase successfully");
  } else {
    Serial.printf("Failed to send LDR value: %s\n", fbdo.errorReason().c_str());
  }
}

void waitDataCommand() {

  int packetSize = udp.parsePacket();

  if (packetSize) {
    Serial.print(" Received packet from : ");
    Serial.println(udp.remoteIP());
    Serial.print(" Size : ");
    Serial.println(packetSize);
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len - 1] = 0;
    textData = String(packetBuffer);
    textData.trim();
    Serial.printf("Data From Client: %s\n", textData);
    Serial.println("\n");
    delay(1000);
  }
}
