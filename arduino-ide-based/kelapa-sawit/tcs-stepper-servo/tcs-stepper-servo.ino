#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Stepper.h>
#include <ESP32Servo.h>
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


#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17

const int stepsPerRevolution = 2048;  // change this to fit the number of steps per revolution
const float resolution = 0.17578125;  // put your step resolution here
float currentAngle = 0.0;

Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);
// Inisialisasi sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_60X);

Servo myservo;

const int servoPin = 25;

// Pin GPIO untuk mengontrol LED
const int ledPin = 15;
const int relayPin = 13;
const int switchPin = 4;

float currentClear = 0.0;
float detectionThreshold = 3000.0;

bool objectDetected = false;
bool lastObjectDetected = false;

uint16_t r, g, b, c;

int switchVal = 0;


void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);  // Set pin GPIO 15 sebagai output
  kedip(2, 0.5);
  pinMode(switchPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  if (tcs.begin()) {
    Serial.println("TCS34725 ditemukan!");
  } else {
    Serial.println("TCS34725 tidak ditemukan. Silakan periksa koneksi.");
    kedip(3, 1);
    while (1)
      ;  // Stop eksekusi jika sensor tidak ditemukan
  }



  myStepper.setSpeed(8);
  resetStepper();

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);  // Standard 50hz servo
  myservo.attach(servoPin, 500, 2400);

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

  kedip(1, 1);
  digitalWrite(ledPin, HIGH);
}

void loop() {

  tcs.getRawData(&r, &g, &b, &c);
  currentClear = c;

  Serial.print("currentClear: ");
  Serial.println(currentClear);


  if (currentClear > detectionThreshold) {
    objectDetected = true;
  } else {
    objectDetected = false;
  }


  if (objectDetected && lastObjectDetected != objectDetected) {

    Serial.println("Objek terdeteksi");

     delay(2000);
     tcs.getRawData(&r, &g, &b, &c);

     // Konversi nilai raw ke format RGB (0-255)
    float red = r;
    float green = g;
    float blue = b;
    float sum = r + g + b;

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

    
    String warna = identifyColor((int)red, (int)green, (int)blue);
    Serial.println(warna);
    kedip(2, 0.5);
    moveCircle(warna);
    delay(1000);
    myservo.write(200);
    delay(1000);
    myservo.write(0);
    updateRGB((int)red, (int)green, (int)blue, warna);
    delay(1000);
    digitalWrite(ledPin, HIGH);
  }


  lastObjectDetected = objectDetected;

  delay(500);  // Baca setiap 1 detik
}

void kedip(int ulang, float detik) {
  for (int i = 0; i < ulang; i++) {
    digitalWrite(ledPin, HIGH);
    delay(detik * 1000);
    digitalWrite(ledPin, LOW);
    delay(detik * 1000);
  }
}


// Fungsi untuk membedakan warna berdasarkan nilai RGB
String identifyColor(int r, int g, int b) {
  if (r < 120 && g <= 100 && (b - g > -30) && (b - r < 80)) {
    return "Ungu Kehitaman";
  } else if (r < g && g - r > 30 && g > 50  && g <= 200 && b < g && (g - b) > 30) {
    return "Hijau Gelap";
  } else if (r <= 140 && g <= 120 && (g - b) <= 100 && ((r - g) <= 100 && r > g)) {
    return "Jingga Merah Kehitaman";
  } else if (r >= 120 && g <= 60 && (g - b) <= 20 && (r - g > 100)) {
    return "Merah";
  } else if (r > 140 && (g >= b) && (r - g > g - b)) {
    return "Jingga Kemerahan";
   } else if ((g-b) > 50 && (r >= g || g-r < 10) && b <= 200) {

    return "Kuning";
  } else {
    return "Tidak Dikenal";
  }
}


void moveCircle(String cl) {
  digitalWrite(relayPin, LOW);
  if (cl == "Merah" || cl == "Jingga Merah Kehitaman") {
    myStepper.step(step_degree(360));
  } else if (cl == "Ungu Kehitaman") {
    myStepper.step(step_degree(60));
  } else if (cl == "Hijau Gelap") {
    myStepper.step(step_degree(120));
  } else if (cl == "Jingga Kemerahan") {
    myStepper.step(step_degree(180));
  } else if (cl == "Kuning") {
    myStepper.step(step_degree(240));
  } else if (cl == "Tidak Dikenal") {
    myStepper.step(step_degree(300));
  }
  digitalWrite(relayPin, HIGH);
}


int step_degree(float desired_degree) {
  Serial.print("CURRENT : ");
  Serial.println(currentAngle);
  int step_moved = (desired_degree - currentAngle) / resolution;
  currentAngle = desired_degree;
  return step_moved;
}

void resetStepper() {

  while (switchVal == 0) {
    switchVal = digitalRead(switchPin);
    Serial.print("SWITCH : ");
    Serial.println(switchVal);
    myStepper.step(1);
    delay(1);
  }

  Serial.println("Stepper Reset Done!");
  digitalWrite(relayPin, HIGH);
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
