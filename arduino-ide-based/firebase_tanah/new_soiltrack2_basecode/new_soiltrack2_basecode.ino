#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define WIFI_SSID "SAUMATA TEKNOSAINS GLOBAL"
#define WIFI_PASSWORD "11022022"

#define API_KEY "AIzaSyDtg593BAXgGTY8h3yNWklaTilrC5_qDAI"
#define DATABASE_URL "https://soiltrack-4a415-default-rtdb.firebaseio.com/"
#define USER_EMAIL "junitasalonga@gmail.com"
#define USER_PASSWORD "soiltrack123"

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
unsigned long sendDelay = 5000; /*pengiriman data ke firebase setiap 5 detik*/

// Inisiasi LCD //
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Inisiasi Sensor Soil Moisture //
#define moistPin 35
#define relayPin 12
bool pumpStatus;
int moistInput;
float moistVal;

// pH Sensor
#define dmsPin  13
#define phPin 34
int phInput;
float phVal;

// Temperature Sensor
#include <OneWire.h>
#include <DallasTemperature.h>
const int oneWireBus = 15;
OneWire oneWire(oneWireBus);
DallasTemperature tempSensors(&oneWire);
float tempVal;

void setup() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("INITIALIZING");
  lcd.setCursor(0, 1); lcd.print("SYSTEM");

  Serial.begin(9600);
  pinMode(moistPin, INPUT);
  pinMode(phPin, INPUT);

  // Aktifkan DMS
  pinMode(dmsPin, OUTPUT);
  digitalWrite(dmsPin, HIGH);

  // Initialize the LCD
  lcd.init();
  lcd.backlight();

  // Initialize temperature sensor
  tempSensors.begin();

  // Inisiasi Relay
  pinMode (relayPin, OUTPUT);
  digitalWrite (relayPin, HIGH);

  //Initialize WIFI
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("CONNECTING");
  lcd.setCursor(0, 1); lcd.print("TO WIFI");

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

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("IP ADDRESS:");
  lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
  delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("CONNECTING TO");
  lcd.setCursor(0, 1); lcd.print("FIREBASE");
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024);
  fbdo.setResponseSize(2048);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;
}

void loop() {
  Serial.println("=================================================================");
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("M: ");
  lcd.setCursor(9, 0); lcd.print("pH: ");
  lcd.setCursor(0, 1); lcd.print("T: ");
  // Soil Moisture
  moistInput = analogRead(moistPin); // read the analog value from sensor
  moistVal = map(moistInput, 2900, 1300, 0, 100);

  if (moistVal >= 100) {
    moistVal = 100;
  }
  if (moistVal <= 0) {
    moistVal = 0;
  }

  Serial.print("Kelembaban  : " + String(moistVal, 0) + "%" + " / ADC(" + moistInput + ")");
  lcd.setCursor(0, 0); lcd.print("M: ");   lcd.print(String(moistVal, 0)); lcd.print("%");

  if (moistVal < 50 ) {
    Serial.println(", Pompa Aktif");
    digitalWrite(relayPin, LOW);
    pumpStatus = true;
  }
  if (moistVal >= 50) {
    Serial.println(", Pompa Mati");
    digitalWrite(relayPin, HIGH);
    pumpStatus = false;
  }

  // Temperature Sensor
  tempSensors.requestTemperatures();
  tempVal = tempSensors.getTempCByIndex(0);

  Serial.println("Suhu        : " + String(tempVal,  1) + "ºC");
  lcd.setCursor(0, 1); lcd.print("T: ");   lcd.print(String(tempVal, 1));  lcd.print("*C");

  // pH Sensor
  digitalWrite(dmsPin, LOW);
  delay(5 * 1000);
  phInput = 0.2498 * analogRead(phPin) - 0.0035;
  digitalWrite(dmsPin, HIGH);

  phVal = (-0.0693 * phInput) + 7.3855;
  if (phVal < 0.0) {
    phVal = 0.0;
  }
  if (phVal > 14.0) {
    phVal = 14.0;
  }
  Serial.println("pH Tanah    : " + String(phVal, 1) + " / ADC(" + String(phInput) + ")");
  lcd.setCursor(9, 0); lcd.print("pH: ");  lcd.print(String(phVal, 1));

  float moistValue  = (String(moistVal, 0)).toFloat();
  float phValue     = (String(phVal,    1)).toFloat();
  float tempValue   = (String(tempVal,  1)).toFloat();

  if (Firebase.ready() && (millis() - sendDataPrevMillis > sendDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    lcd.setCursor(12, 1); lcd.print("SEND");
    Serial.println();
    Serial.printf("Send Status Pompa: %s\n", Firebase.RTDB.setBool  (&fbdo, F("/test/pompa"), pumpStatus)       ? "sent" : fbdo.errorReason().c_str());
    Serial.printf("Send Kelembaban  : %s\n", Firebase.RTDB.setFloat (&fbdo, F("/test/kelembaban"), moistValue)  ? "sent" : fbdo.errorReason().c_str());
    Serial.printf("Send pH Tanah    : %s\n", Firebase.RTDB.setFloat (&fbdo, F("/test/phTanah"), phValue)        ? "sent" : fbdo.errorReason().c_str());
    Serial.printf("Send Suhu        : %s\n", Firebase.RTDB.setFloat (&fbdo, F("/test/suhu"), tempValue)         ? "sent" : fbdo.errorReason().c_str());
  }
  Serial.println("=================================================================");
  Serial.println();
  delay(3 * 1000);
}
