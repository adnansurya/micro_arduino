#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "IDEAlist"

#define DATABASE_SECRET "n6AqF87ZMBGbNww3rwYm4LANCPHL3r4HxtEhvZtC"
#define DATABASE_URL "https://colordetectedpalmfruit-default-rtdb.firebaseio.com"

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
LegacyToken dbSecret(DATABASE_SECRET);


// Inisialisasi sensor dengan waktu integrasi dan gain
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_60X);

// Fungsi untuk membedakan warna berdasarkan nilai RGB
String identifyColor(int r, int g, int b) {
  if (r <= 100 && g <= 50 && b <= 100 && (g < r || g <= b)) {
    return "Ungu Kehitaman";
  } else if (r < g && g >= 100 && b <= g) {
    return "Hijau Gelap";
  } else if (r <= 140 && g <= 120 && (g - b) <= 100 && ((r - g) <= 100 && r > g)) {
    return "Jingga Merah Kehitaman";
  } else if (r >= 120 && g <= 60 && (g - b) <= 20 && (r - g > 100)) {
    return "Merah";
  } else if (r > 140 && (g - b) > 20 && (r - g > g - b)) {
    return "Jingga Kemerahan";
  } else if ((g - b) > 50 && r >= g && b <= 200) {
    return "Kuning";
  } else {
    return "Tidak Dikenal";
  }
}

void setup() {
  Serial.begin(115200);

  if (tcs.begin()) {
    Serial.println("TCS34725 ditemukan!");
  } else {
    Serial.println("TCS34725 tidak ditemukan. Silakan periksa koneksi.");
    while (1)
      ;  // Stop eksekusi jika sensor tidak ditemukan
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  ssl.setInsecure();
#if defined(ESP8266)
  ssl.setBufferSizes(1024, 1024);
#endif

  // Initialize the authentication handler.
  initializeApp(client, app, getAuth(dbSecret));

  // Binding the authentication handler with your Database class object.
  app.getApp<RealtimeDatabase>(Database);

  // Set your database URL
  Database.url(DATABASE_URL);

  // In sync functions, we have to set the operating result for the client that works with the function.
  client.setAsyncResult(result);
}

void loop() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // Konversi nilai raw ke format RGB (0-255)
  float red = r;
  float green = g;
  float blue = b;
  float sum = c;

  if (sum > 0) {
    red = (red / sum) * 255.0;
    green = (green / sum) * 255.0;
    blue = (blue / sum) * 255.0;
  }

  Serial.print("R: ");
  Serial.print((int)red);
  Serial.print(" ");
  Serial.print("G: ");
  Serial.print((int)green);
  Serial.print(" ");
  Serial.print("B: ");
  Serial.println((int)blue);
  Serial.print("C: ");
  Serial.println((int)sum);

  // Identifikasi warna berdasarkan nilai RGB
  String detectedColor = identifyColor((int)red, (int)green, (int)blue);
  Serial.print("Warna terdeteksi: ");
  Serial.println(detectedColor);
  updateRGB((int)red, (int)green, (int)blue, detectedColor);

  delay(1000);  // Baca setiap 1 detik
}

void printError(int code, const String &msg) {
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}


void updateRGB(int r, int g, int b, String text) {
  Serial.print("Set JSON... ");

  bool status = Database.set<object_t>(client, "/rgb", object_t("{\"r\":" + String(r) + ",\"g\":" + String(g) + ",\"b\":" + String(b) + ",\"color\":\"" + text + "\"}"));
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());
}
