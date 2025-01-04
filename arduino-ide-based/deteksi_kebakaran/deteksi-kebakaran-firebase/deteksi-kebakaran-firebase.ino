#include "DHT.h"

#define flamePin 14
#define dhtPin 13
#define mq2Pin 34

#define dhtType DHT11

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

// Konfigurasi WiFi
const char* WIFI_SSID = "MIKRO";         // Ganti dengan SSID WiFi Anda
const char* WIFI_PASSWORD = "1DEAlist";  // Ganti dengan password WiFi Anda

#define DATABASE_URL "https://firealert-3280e-default-rtdb.firebaseio.com/"

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
NoAuth noAuth;

// Konstanta untuk konversi sensor MQ-2 (contoh nilai berdasarkan datasheet dan kalibrasi)
const float RL = 10.0;                           // Beban resistor dalam kilo ohm (kΩ)
const float Ro = 9.8;                            // Nilai Ro hasil kalibrasi dalam kilo ohm (kΩ)
const float LPGCurve[3] = { 2.3, 0.21, -0.47 };  // Kurva LPG {log10(200ppm), slope, intercept}

DHT dht(dhtPin, dhtType);
int adaApi = 0;
bool status;

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

  // Menghitung nilai resistansi sensor
  float sensorResistance = calculateResistance(analogValue);

  // Menghitung konsentrasi gas LPG dalam ppm
  float lpgConcentration = calculatePPM(sensorResistance);

  // Tampilkan nilai di Serial Monitor
  Serial.print("Nilai analog MQ-2: ");
  Serial.println(analogValue);
  Serial.print("Resistansi sensor: ");
  Serial.print(sensorResistance);
  Serial.println(" kΩ");
  Serial.print("Konsentrasi LPG: ");
  Serial.print(lpgConcentration);
  Serial.println(" ppm");

  adaApi = !digitalRead(flamePin);
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  if (isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.print("Api: ");
  Serial.print(adaApi);
  Serial.print(F("\tSuhu: "));
  Serial.print(t);
  Serial.println(F("°C "));

  // Kirim data ke Firebase
  Serial.print("Set int... ");
  status = Database.set<int>(client, "/sensors/flame", adaApi);
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  Serial.print("Set float... ");
  status = Database.set<number_t>(client, "/sensors/suhu", number_t(t,2));
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  Serial.print("Set float... ");
  status = Database.set<number_t>(client, "/sensors/lpg", number_t(lpgConcentration,2));
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  delay(5000);
}

float calculatePPM(float sensorResistance) {
  float ratio = sensorResistance / Ro;  // Rasio Rs/Ro
  return pow(10, ((log10(ratio) - LPGCurve[2]) / LPGCurve[1]));
}

float calculateResistance(int adcValue) {
  float voltage = (float)adcValue / 4095.0 * 3.3;  // Konversi nilai ADC ke tegangan (ESP32 ADC 12-bit)
  return (3.3 - voltage) * RL / voltage;           // Hitung nilai resistansi sensor
}

void printError(int code, const String& msg) {
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}
