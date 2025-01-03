#include "DHT.h"

#define flamePin 14
#define dhtPin 13
#define mq2Pin 34

#define dhtType DHT11

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

// Konfigurasi WiFi
// const char* WIFI_SSID = "MIKRO";         // Ganti dengan SSID WiFi Anda
// const char* WIFI_PASSWORD = "1DEAlist";  // Ganti dengan password WiFi Anda
const char* WIFI_SSID = "hotspot";         // Ganti dengan SSID WiFi Anda
const char* WIFI_PASSWORD = "namakusaja1";  // Ganti dengan password WiFi Anda

#define DATABASE_URL "https://firealert-3280e-default-rtdb.firebaseio.com/"
#define ID_SENSOR "-OEgt_pjDc2J_l3recxI"

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
NoAuth noAuth;


DHT dht(dhtPin, dhtType);
int adaApi = 0;
bool status;

const char* dirFirebase = "/sensors/-OEgt_pjDc2J_l3recxI/";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(flamePin, INPUT);

  dht.begin();

  Serial.print("Menghubungkan ke WiFi");
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
  initializeApp(client, app, getAuth(noAuth));

  // Binding the authentication handler with your Database class object.
  app.getApp<RealtimeDatabase>(Database);

  // Set your database URL
  Database.url(DATABASE_URL);

  // In sync functions, we have to set the operating result for the client that works with the function.
  client.setAsyncResult(result);
}

void loop() {
  // put your main code here, to run repeatedly:

  int analogValue = analogRead(mq2Pin);
  Serial.print("MQ2 Analog: ");
  Serial.println(analogValue);
  int conValue = map(analogValue, 1500, 4096, 0, 10000);
  float persenGas = (float) conValue / 100.0;
  if(persenGas < 0.0){
    persenGas = 0.0;
  }


  adaApi = !digitalRead(flamePin);
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  if (isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    // return;
    t = 0.0;
  }

  // Tampilkan nilai di Serial Monitor
  Serial.print("MQ-2: ");
  Serial.print(persenGas);
  Serial.print(" %\tApi: ");
  Serial.print(adaApi);
  Serial.print(F("\tSuhu: "));
  Serial.print(t);
  Serial.println(F( "°C "));

  // Kirim data ke Firebase
  Serial.print("Set int... ");
  status = Database.set<int>(client, dirFirebase + "flame", adaApi);
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  Serial.print("Set float... ");
  status = Database.set<number_t>(client, dirFirebase + "suhu", number_t(t,2));
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  Serial.print("Set float... ");
  status = Database.set<number_t>(client, dirFirebase + "lpg", number_t(persenGas,2));
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  delay(5000);
}


void printError(int code, const String& msg) {
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}
