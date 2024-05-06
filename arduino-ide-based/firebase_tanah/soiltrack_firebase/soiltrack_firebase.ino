#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// #define WIFI_SSID "SAUMATA TEKNOSAINS GLOBAL"
// #define WIFI_PASSWORD "11022022"

// #define API_KEY "AIzaSyDtg593BAXgGTY8h3yNWklaTilrC5_qDAI"
// #define DATABASE_URL "https://soiltrack-4a415-default-rtdb.firebaseio.com/"
// #define USER_EMAIL "junitasalonga@gmail.com"
// #define USER_PASSWORD "soiltrack123"

#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "IDEAlist"

#define API_KEY "AIzaSyDEGNgsHGBT2jeQIi7kCBnYqjCZmoTPEDo"
#define DATABASE_URL "https://pendeteksi01.firebaseio.com/"


#define suhuPin 32
#define relayPin 17  //VP Pin = 36
#define soilPin 33

#define dmsPin 13
#define phPin 34

LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// Inisialisasi pin data (DQ)
OneWire oneWire(suhuPin);
DallasTemperature sensors(&oneWire);

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
long intervalMillis = 5000;
bool signupOK = false;

float maxAdc = 4095.0;
bool pompaOn = false;

float batasLembab = 50.0;

int phAdc;
float phVal;
float phLast;

void setup() {
  Serial.begin(115200);
  sensors.begin();

  lcd.init();  // initialize the lcd
  lcd.backlight();

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  pinMode(dmsPin, OUTPUT);
  digitalWrite(dmsPin, HIGH);

  lcdPrintAll("Connecting to:", String(WIFI_SSID), 0);

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

  lcdPrintAll("Connected. IP :", String(WiFi.localIP()), 1500);

  lcdPrintAll("Initialize", "Firebase...", 1000);
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
  Serial.println("----START LOOP----");
  sensors.requestTemperatures();                    // Minta pembacaan suhu
  float temperatureC = sensors.getTempCByIndex(0);  // Ambil suhu dalam derajat Celsius
  Serial.print("Suhu: ");
  Serial.print(temperatureC);
  Serial.println(" °C");

  int soilAdc = maxAdc - analogRead(soilPin);  // read the analog value from sensor
  float soilVoltage = (float)soilAdc / maxAdc * 3.3;
  float soilPercent = (float)soilAdc / maxAdc * 100;

  Serial.print("Moisture (Adc): ");
  Serial.print(soilAdc);
  Serial.print("\tPercent : ");
  Serial.print(soilPercent);
  Serial.println(" %");

  // delay(500);
  lcdPrintAll("T: " + String(temperatureC), "M: " + String(soilPercent) + " %", 500);

  if (soilPercent > batasLembab) {
    digitalWrite(relayPin, HIGH);
    pompaOn = false;
  } else {
    digitalWrite(relayPin, LOW);
    pompaOn = true;
  }

  digitalWrite(dmsPin, LOW);
  delay(10 * 1000);

  phAdc = analogRead(phPin);
  phVal = (-0.0139 * phAdc) + 7.7851;
  if (phVal != phLast) {
    phLast = phVal;
  }

  lcdPrintAll("PH : " + String(phVal), "", 500);

  Serial.print("PH (Adc) : ");
  Serial.print(phAdc);
  Serial.print("\tValue : ");
  Serial.println(phVal);

  if (Firebase.ready() && (millis() - sendDataPrevMillis > intervalMillis || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    lcdPrintAll("SEND TO", "FIREBASE", 0);
    Serial.println();
    Serial.printf("Send Status Pompa: %s\n", Firebase.RTDB.setBool(&fbdo, F("/test/pompa"), pompaOn) ? "sent" : fbdo.errorReason().c_str());
    Serial.printf("Send Kelembaban  : %s\n", Firebase.RTDB.setFloat(&fbdo, F("/test/kelembaban"), soilPercent) ? "sent" : fbdo.errorReason().c_str());
    Serial.printf("Send pH Tanah    : %s\n", Firebase.RTDB.setFloat(&fbdo, F("/test/phTanah"), phVal) ? "sent" : fbdo.errorReason().c_str());
    Serial.printf("Send Suhu        : %s\n", Firebase.RTDB.setFloat(&fbdo, F("/test/suhu"), temperatureC) ? "sent" : fbdo.errorReason().c_str());
  }

  digitalWrite(dmsPin, HIGH);
  delay(3 * 1000);
}

void lcdPrintAll(String baris1, String baris2, int jeda) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2);
  delay(jeda);
}
