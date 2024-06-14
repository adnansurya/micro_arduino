
#include <Arduino.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "IDEAlist"

#define API_KEY "AIzaSyDX1sxfvOHijumSJSM37fhrSmJUrLgsn1Y"
#define DATABASE_URL "https://antihama-e3a00-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define GMT_OFFSET 8

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


bool bunyiManual;

void setup() {

  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

   Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  // auth.user.email = USER_EMAIL;
  // auth.user.password = USER_PASSWORD;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;

   
}


void loop(){
  Serial.printf("Get Bunyi Manual... %s\n", Firebase.RTDB.getBool(&fbdo, F("/bunyiManual"), &bunyiManual) ? bunyiManual ? "true" : "false" : fbdo.errorReason().c_str());
  delay(1000);
}