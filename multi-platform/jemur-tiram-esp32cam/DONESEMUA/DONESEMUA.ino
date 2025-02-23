#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <WiFiUDP.h>

// Library dan fungsi Firebase
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Sensor dan koneksi Wi-Fi
#define WIFI_SSID "Can jie"
#define WIFI_PASSWORD "qwerty015"
#define API_KEY "AIzaSyAEJvUK8RGqR-tI6QyRb2bUMF6Y2mUj3D4"
#define DATABASE_URL "https://thelast-e2d6d-default-rtdb.firebaseio.com/"

Adafruit_SHT31 sht31 = Adafruit_SHT31();
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define LDRPIN 34
int ldrValue = 0;
int udpPort = 202018202081;
WiFiUDP udp;
float kelembapan = 0;

void setup() {
  Serial.begin(115200);

  if (!sht31.begin(0x44)) { // Alamat I2C dari SHT31
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

  // Mulai UDP
  udp.begin(udpPort);
  Serial.printf("UDP Listening on port %d\n", udpPort);
}

void loop() {
  bacaLDR();
  bacaSHT();
  receiveDataSuhu();

  if (Firebase.ready()) {
    static unsigned long sendDataPrevMillis = 0;
    if (millis() - sendDataPrevMillis > 7500) {
      sendDataPrevMillis = millis();
      if (Firebase.RTDB.setInt(&fbdo, "sensor/cahaya", ldrValue)) {
        Serial.println("LDR value sent to Firebase successfully");
      } else {
        Serial.printf("Failed to send LDR value: %s\n", fbdo.errorReason().c_str());
      }
    }
  }
}

void bacaLDR() {
  ldrValue = analogRead(LDRPIN);
  Serial.print("Nilai LDR: ");
  Serial.println(ldrValue);
  delay(3000);
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
  
  static unsigned long sendDataPrevMillis = 0;
  if (millis() - sendDataPrevMillis > 7500) {
    sendDataPrevMillis = millis();
    if (Firebase.RTDB.setFloat(&fbdo, "sensor/kelembapan", kelembapan)) {
      Serial.println("Humidity value sent to Firebase successfully");
    } else {
      Serial.printf("Failed to send humidity value: %s\n", fbdo.errorReason().c_str());
    }
  }
  delay(3000);
}

void receiveDataSuhu() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    byte data[sizeof(float)];
    udp.read(data, sizeof(data));

    float temperatureFloat;
    memcpy(&temperatureFloat, data, sizeof(temperatureFloat));

    Serial.print("Received data: ");
    Serial.println(temperatureFloat);

    if (Firebase.RTDB.setFloat(&fbdo, "sensor/suhu", temperatureFloat)) {
      Serial.println("Temperature value sent to Firebase successfully");
    } else {
      Serial.printf("Failed to send temperature value: %s\n", fbdo.errorReason().c_str());
    }
  }
  delay(3000);
}
