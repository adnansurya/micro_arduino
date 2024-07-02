
#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager


//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "TP-Link_2C04"
#define WIFI_PASSWORD "92120266"

// Insert Firebase project API Key
#define API_KEY "AIzaSyBc6X4IjEKCtd1pw_2Cc6DIkrmXbqT3Hmg"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://safepil-default-rtdb.asia-southeast1.firebasedatabase.app"

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
long intervalMillis = 5000;
int count = 0;
bool signupOK = false;

String deviceId = "device_11";
String fbDir = "main/realtime/" + deviceId;

WiFiManager wm;
bool res;

String str_a, str_n;
float nilai_a, nilai_n;

void setup() {
  Serial.begin(115200);

  res = wm.autoConnect("AutoConnectAP");  // anonymous ap
  if (!res) {
    Serial.println("Failed to connect");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);



  Serial.printf("Get Nilai N... %s\n", Firebase.RTDB.getString(&fbdo, F("/main/nilai_n"), &str_n) ? String(str_n).c_str() : fbdo.errorReason().c_str());
  Serial.printf("Get Nilai A... %s\n", Firebase.RTDB.getString(&fbdo, F("/main/nilai_a"), &str_a) ? String(str_a).c_str() : fbdo.errorReason().c_str());
  nilai_n = str_n.toFloat();
  nilai_a = str_a.toFloat();
}

void loop() {
  long rssiVal = WiFi.RSSI();
  float jarak = rssiToMeter(rssiVal, nilai_a, nilai_n);
  Serial.println(jarak);

  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > intervalMillis || sendDataPrevMillis == 0)) {
    Serial.print("RSSI  : ");
    Serial.println(rssiVal);
    sendDataPrevMillis = millis();
    // Write an Int number on the database path test/int
    if (Firebase.RTDB.setInt(&fbdo, fbDir + "/rssi", rssiVal)) {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.setFloat(&fbdo, fbDir + "/meter", jarak)) {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

float rssiToMeter(long rssiUkur, float nilaiA, float nilaiN) {
  float meterHasil = 0.0;

  float pembilang = nilaiA - (float) rssiUkur ;
  float penyebut = 10 * nilaiN;
  float pangkat = pembilang / penyebut;
  
  meterHasil = pow(10, pangkat);

  return abs(meterHasil);
}
