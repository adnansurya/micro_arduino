
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

#include "DFRobotDFPlayerMini.h"

/* 1. Define the WiFi credentials */
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "IDEAlist"

#define API_KEY "AIzaSyDX1sxfvOHijumSJSM37fhrSmJUrLgsn1Y"
#define DATABASE_URL "https://antihama-e3a00-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define GMT_OFFSET 8

#define pinMotion 34

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;


bool bunyiManual = false;
bool lastBunyiManual = false;

int adaGerak;

DFRobotDFPlayerMini mp3;

unsigned long mainTimer = 1000;
unsigned long mainTimerChecker = 0;

void setup() {

  Serial2.begin(9600);
  Serial.begin(115200);
  pinMode(pinMotion, INPUT);

  if (!mp3.begin(Serial2)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true)
      ;
  }
  Serial.println(F("DFPlayer Mini online."));

  mp3.setTimeOut(500);  //Set serial communictaion time out 500ms

  //----Set volume----
  mp3.volume(20);  //Set volume value (0~30).

  //----Set different EQ----
  mp3.EQ(DFPLAYER_EQ_NORMAL);

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


void loop() {

  if (millis() > mainTimerChecker + mainTimer) {
    adaGerak = digitalRead(pinMotion);

    Serial.print("Ada Gerak : ");
    Serial.println(adaGerak);

    if (adaGerak) {
      Serial.printf("Set bool... %s\n", Firebase.RTDB.setBool(&fbdo, F("/adaHama"), true) ? "ok" : fbdo.errorReason().c_str());
    } else {
      Serial.printf("Set bool... %s\n", Firebase.RTDB.setBool(&fbdo, F("/adaHama"), false) ? "ok" : fbdo.errorReason().c_str());
    }

    Serial.printf("Get Bunyi Manual... %s\n", Firebase.RTDB.getBool(&fbdo, F("/bunyiManual"), &bunyiManual) ? bunyiManual ? "true" : "false" : fbdo.errorReason().c_str());

    if (bunyiManual != lastBunyiManual) {
      if (bunyiManual == true) {
        Serial.println("Perintah Membunyikan Secara Manual!");
        mp3.start();       
        mp3.play(1); 
        delay(1000);  
        mp3.loop(1);   

      } else {
        mp3.pause();
        Serial.println("Berhenti Manual");
      }
    }

    lastBunyiManual = bunyiManual;
    mainTimerChecker = millis();
  }
  
}