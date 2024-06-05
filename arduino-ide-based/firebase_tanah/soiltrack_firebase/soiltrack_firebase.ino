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

#include <NTPClient.h>
#include <WiFiUdp.h>



//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// #define WIFI_SSID "SAUMATA TEKNOSAINS GLOBAL"
// #define WIFI_PASSWORD "11022022"

#define API_KEY "AIzaSyDtg593BAXgGTY8h3yNWklaTilrC5_qDAI"
#define DATABASE_URL "https://soiltrack-4a415-default-rtdb.firebaseio.com/"
// #define USER_EMAIL "junitasalonga@gmail.com"
// #define USER_PASSWORD "soiltrack123"

#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "IDEAlist"

// #define API_KEY "AIzaSyDEGNgsHGBT2jeQIi7kCBnYqjCZmoTPEDo"
// #define DATABASE_URL "https://pendeteksi01.firebaseio.com/"


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

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

time_t epochTime;
String weekDay, formattedTime, currentDate, waktu, tStamp;
int currentYear = 0;

#define GMT_OFFSET 8

//Nama hari
String weekDays[7] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };

//Nama bulan
String months[12] = { "Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli", "Agustus", "September", "Oktober", "November", "Desember" };


unsigned long sendDataPrevMillis = 0;
long intervalMillis = 5000;
bool signupOK = false;

float maxAdc = 4095.0;
bool pompaOn = false;

float batasLembab = 50.0;

int phAdc;
float phVal = 0.0;
float phLast = 0.0;
float toleransi = 7;

float temperatureC = 0.0;
float soilPercent = 0.0;

unsigned long lcdUpdateTime = 0;
unsigned long lcdUpdateInterval = 2500;

unsigned long phUpdateTime = 0;
unsigned long phUpdateInterval = 10000;

unsigned long dmsUpdateTime = 0;
unsigned long dmsUpdateInterval = 3000;

bool dmsOn = true;
bool phDisplay = false;
bool dmsStart = false;

void setup() {
  Serial.begin(115200);
  sensors.begin();

  lcd.init();  // initialize the lcd
  lcd.backlight();

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  pinMode(dmsPin, OUTPUT);
  digitalWrite(dmsPin, HIGH);


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to:");
  lcd.setCursor(0, 1);
  lcd.print(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    lcd.print(".");
    delay(500);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected. IP : ");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(GMT_OFFSET * 3600);
  delay(1000);



  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing");
  lcd.setCursor(0, 1);
  lcd.print("Firebase.....");
  delay(1000);
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
  unsigned long currentTime = millis();

  if (currentTime - lcdUpdateTime >= lcdUpdateInterval) {
    sensors.requestTemperatures();              // Minta pembacaan suhu
    temperatureC = sensors.getTempCByIndex(0);  // Ambil suhu dalam derajat Celsius
    Serial.print("Suhu: ");
    Serial.print(temperatureC);
    Serial.println(" °C");

    int soilAdc = maxAdc - analogRead(soilPin);  // read the analog value from sensor
    float soilVoltage = (float)soilAdc / maxAdc * 3.3;
    soilPercent = (float)soilAdc / maxAdc * 100;

    Serial.print("Moisture (Adc): ");
    Serial.print(soilAdc);
    Serial.print("\tPercent : ");
    Serial.print(soilPercent);
    Serial.println(" %");

    // delay(500);
    lcd.clear();
    if (phDisplay == false) {
      lcd.setCursor(0, 0);
      lcd.print("T: ");
      lcd.print(temperatureC);
      lcd.print(" ");
      lcd.print((char)223);
      lcd.print("C");
      lcd.setCursor(0, 1);
      lcd.print("M: ");
      lcd.print(soilPercent);
      lcd.print(" %");
      phDisplay = true;
    } else {
      lcd.setCursor(0, 0);
      lcd.print("PH: ");
      if(dmsStart){
        lcd.print(phLast);
      }else{
        lcd.print("<Not Ready>");
      }
      
      phDisplay = false;
    }

    if (soilPercent > batasLembab) {
      digitalWrite(relayPin, HIGH);
      pompaOn = false;
    } else {
      digitalWrite(relayPin, LOW);
      pompaOn = true;
    }
    lcdUpdateTime = currentTime;
  }

  if ((currentTime - phUpdateTime >= phUpdateInterval) && dmsOn == true) {
    phAdc = analogRead(phPin);
    phVal = (-0.0139 * phAdc) + 7.7851;
    if (phVal != phLast) {
      phLast = anomaliChecker(phLast, phVal);
      if(dmsStart == false){
        dmsStart = true;    
      }
    }

    Serial.print("PH (Adc) : ");
    Serial.print(phAdc);
    Serial.print("\tValue : ");
    Serial.println(phVal);

    Serial.println("DMS Off");
    dmsOn = false;
    digitalWrite(dmsPin, HIGH);
    // phUpdateTime = currentTime;
    dmsUpdateTime = currentTime;
  }

  if ((currentTime - dmsUpdateTime >= dmsUpdateInterval) && dmsOn == false) {

    Serial.println("DMS Standby");
    digitalWrite(dmsPin, LOW);
    dmsOn = true;


    lcd.setCursor(14, 1);
    lcd.print("F");
    lcd.print((char)126);

    Serial.println("SEND TO FIREBASE");
    if (Firebase.ready()) {
      // Serial.printf("Send Status Pompa: %s\n", Firebase.RTDB.setBool(&fbdo, F("/test/pompa"), pompaOn) ? "sent" : fbdo.errorReason().c_str());
      // Serial.printf("Send Kelembaban  : %s\n", Firebase.RTDB.setFloat(&fbdo, F("/test/kelembaban"), soilPercent) ? "sent" : fbdo.errorReason().c_str());
      // Serial.printf("Send pH Tanah    : %s\n", Firebase.RTDB.setFloat(&fbdo, F("/test/phTanah"), phVal) ? "sent" : fbdo.errorReason().c_str());
      // Serial.printf("Send Suhu        : %s\n", Firebase.RTDB.setFloat(&fbdo, F("/test/suhu"), temperatureC) ? "sent" : fbdo.errorReason().c_str());
      getWaktu();
      sendRTDB(pompaOn, soilPercent, temperatureC, phLast);
    }

    // dmsUpdateTime = currentTime;
    phUpdateTime = currentTime;
  }



  // if (Firebase.ready() && (millis() - sendDataPrevMillis > intervalMillis || sendDataPrevMillis == 0)) {
  //   sendDataPrevMillis = millis();

  // }
}


void sendRTDB(bool pompa, float lembab, float suhu, float ph) {
  FirebaseJson json;

  json.set("pompa", pompa);
  json.set("kelembaban", lembab);
  json.set("phTanah", ph);
  json.set("suhu", suhu);
  json.set("waktu", waktu);
  json.set("timestamp", tStamp);

  
  if (Firebase.RTDB.updateNode(&fbdo, "/realtime", &json)) {

    Serial.println(fbdo.dataPath());

    Serial.println(fbdo.pushName());

    Serial.println(fbdo.dataPath() + "/" + fbdo.pushName());

  } else {
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.RTDB.pushJSON(&fbdo, "/histori", &json)) {

    Serial.println(fbdo.dataPath());

    Serial.println(fbdo.pushName());

    Serial.println(fbdo.dataPath() + "/" + fbdo.pushName());

  } else {
    Serial.println(fbdo.errorReason());
  }
}

void getWaktu() {

  timeClient.update();

  epochTime = timeClient.getEpochTime();

  tStamp = String(epochTime);

  formattedTime = timeClient.getFormattedTime();

  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();

  weekDay = weekDays[timeClient.getDay()];

  //Get a time structure
  struct tm *ptm = gmtime((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  String currentMonthName = months[currentMonth - 1];

  currentYear = ptm->tm_year + 1900;

  //Print complete date:
  currentDate = String(monthDay) + "-" + String(currentMonth) + "-" + String(currentYear);

  waktu = weekDay + ", " + currentDate + " " + formattedTime + " WITA";

  Serial.print("->GetWaktu: ");
  Serial.println(waktu);
}

float anomaliChecker(float before, float after){
  float nilaiPh;
  if(after > 14 || after < 0){
    nilaiPh = before;
  }else{
    nilaiPh = after;
  }
  return nilaiPh; 
}
