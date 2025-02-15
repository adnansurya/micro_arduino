#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "ACS712.h"

#define FLASH_LED_PIN 2
#define SOLENOID_PIN 26
#define LAMP_PIN 27
#define LDR_PIN 34
#define ARUS_PIN 35


// Konfigurasi WiFi
#define WIFI_SSID "MIKRO"         // Ganti dengan SSID WiFi Anda
#define WIFI_PASSWORD "1DEAlist"  // Ganti dengan password WiFi Anda

#define DATABASE_URL "https://pintucerdas-4b1ae-default-rtdb.asia-southeast1.firebasedatabase.app/"

String dirPath = "/logs";
time_t tstamp;

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
NoAuth noAuth;

ACS712 sensor(ACS712_05B, ARUS_PIN);

const int lightLimitUpper = 3500;
const int lightLimitLower = 1000;



String statKunci, statLampu, statCommand, statStep, lastStep;
String commandDir = "/speech_result";
String lockDir = "/kunci";
String stepDir = "/step";
String lampDir = "/lamp";


int lightState = 0;
int lastLightState = 0;
int adcLdr;
float arus = 0.0;

void setup() {
  // put your setup code here, to run once:
  // Init Serial Monitor
  Serial.begin(115200);

  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(LAMP_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  digitalWrite(SOLENOID_PIN, HIGH);
  digitalWrite(LAMP_PIN, HIGH);
  digitalWrite(FLASH_LED_PIN, LOW);

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // ledBlink(2, 100);
  Serial.println();

  Serial.print("Retrieving time: ");
  configTime(0, 0, "id.pool.ntp.org", "pool.ntp.org");  // get UTC time via NTP
  tstamp = getTimestamp();

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  ssl.setInsecure();
#if defined(ESP8266)
  ssl.setBufferSizes(1024, 1024);
#endif

  // Initialize the authentication handler.
  initializeApp(client, app, getAuth(noAuth));

  // Binding the authentication handler with your Database class object.
  app.getApp<RealtimeDatabase>(Database);

  // Set your database URL
  Database.url(DATABASE_URL);

  // In sync functions, we have to set the operating result for the client that works with the function.
  client.setAsyncResult(result);



  sensor.calibrate();

  digitalWrite(FLASH_LED_PIN, HIGH);
}

time_t getTimestamp() {
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  return now;
}


void loop() {
  // put your main code here, to run repeatedly:

  statKunci = getStringFb(lockDir);
  Serial.print("statKunci: ");
  Serial.println(statKunci);

  statLampu = getStringFb(lampDir);

  if (statKunci == "terkunci") {
    digitalWrite(SOLENOID_PIN, HIGH);  //solenoid dibuka
  } else if (statKunci == "terbuka") {
    digitalWrite(SOLENOID_PIN, LOW);  //solenoid dibuka
  }

  if (statLampu == "on") {
    digitalWrite(LAMP_PIN, LOW);  //solenoid dibuka
  } else if (statLampu == "off") {
    digitalWrite(LAMP_PIN, HIGH);  //solenoid dibuka
  }

  statCommand = getStringFb(commandDir);
  statCommand.toLowerCase();

  statStep = getStringFb(stepDir);



  if (statStep == "locked" && statCommand.indexOf("buka") > -1) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    Serial.println("Solenoid dibuka");

    setStringFb(lockDir, "terbuka");
    setStringFb(stepDir, "unlocked");
  }

  arus = sensor.getCurrentDC();
  Serial.print("Arus: ");
  Serial.println(arus);

  adcLdr = 4096 - analogRead(LDR_PIN);
  Serial.print("LDR: ");
  Serial.println(adcLdr);
  lightState = getLightState(adcLdr);

  if (lightState == 2 && statStep == "unlocked" && lightState != lastLightState) {  //pintu terbuka
    digitalWrite(FLASH_LED_PIN, HIGH);
    Serial.println("Pintu dibuka");
    // delay(2000);
    setStringFb(stepDir, "open");
    setStringFb(lampDir, "on");

  } else if (lightState == 3 && statKunci == "terbuka" && statStep == "open" && lightState != lastLightState) {  //pintu ditutup
    digitalWrite(FLASH_LED_PIN, HIGH);
    Serial.println("Pintu ditutup");
    // delay(2000);
    setStringFb(lampDir, "off");
    setStringFb(lockDir, "terkunci");
    setStringFb(stepDir, "locked");
    setStringFb(commandDir, "-");
  }

  if (statStep != lastStep) {
    pushData("/logs");
  }



  lastStep = statStep;
  lastLightState = lightState;





  delay(1000);
  digitalWrite(FLASH_LED_PIN, LOW);
}


String getStringFb(String fbDir) {
  String str = "";
  Serial.print(fbDir + ": ");
  str = Database.get<String>(client, fbDir);
  if (client.lastError().code() == 0)
    Serial.println(str);
  else
    printError(client.lastError().code(), client.lastError().message());

  str.toLowerCase();

  return str;
}

void setStringFb(String fbDir, String str) {
  Serial.print("Set " + fbDir + " to:  " + str);
  bool status = Database.set<String>(client, fbDir, str);
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());
}

void pushData(String fbDir) {

  tstamp = getTimestamp();
  String datetime = convertTimestampToDateTime(tstamp, 8);

  String objBuilder = "{ \"ldr_value\":" + String(adcLdr) + ", \"current\":" + String(arus) + ", \"timestamp\":" + String(tstamp) + ", \"datetime\":\"" + datetime + "\", \"status\":\"" + statStep + "\"}";
  Serial.print("Push JSON: ");
  Serial.println(objBuilder);
  String name = Database.push<object_t>(client, fbDir, object_t(objBuilder));
  if (client.lastError().code() == 0)
    Firebase.printf("ok, name: %s\n", name.c_str());
  else
    printError(client.lastError().code(), client.lastError().message());
  delay(1000);
}

int getLightState(int adcVal) {
  int stateNum = 0;
  if (adcVal < lightLimitLower) {
    stateNum = 1;
    Serial.println("INDOOR GELAP");
  } else if (adcVal > lightLimitUpper) {
    stateNum = 3;
    Serial.println("INDOOR TERANG");
  } else {
    stateNum = 2;
    Serial.println("OUTDOOR");
  }

  return stateNum;
}

// Fungsi untuk mengonversi timestamp ke format tanggal dan waktu
String convertTimestampToDateTime(unsigned long timestamp, long offsetHour) {
  struct tm timeinfo;
  time_t rawtime = (time_t)(timestamp + (offsetHour * 3600));

  // Konversi timestamp ke struct tm
  localtime_r(&rawtime, &timeinfo);

  // Format hasil menjadi string (YYYY-MM-DD HH:MM:SS)
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);

  return String(buffer);
}

void printError(int code, const String& msg) {
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}
