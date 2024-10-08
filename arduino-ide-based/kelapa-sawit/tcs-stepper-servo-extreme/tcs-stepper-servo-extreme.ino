#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Stepper.h>
#include <ESP32Servo.h>



#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17

const int stepsPerRevolution = 2048;  // change this to fit the number of steps per revolution
const float resolution = 0.17578125;  // put your step resolution here
float currentAngle = 0.0;

Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);
// Inisialisasi sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_1X);

Servo myservo;

const int servoPin = 25;

// Pin GPIO untuk mengontrol LED
const int ledPin = 15;
const int relayPin = 13;
const int switchPin = 23;

float currentClear = 0.0;
float detectionThreshold = 30.0;

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

  kedip(1, 1);
  digitalWrite(ledPin, HIGH);
}

void loop() {

  tcs.getRawData(&r, &g, &b, &c);
  currentClear = c;

  Serial.print("currentClear: ");
  Serial.println(currentClear);

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





  if (currentClear > detectionThreshold) {
    objectDetected = true;
  } else {
    objectDetected = false;
  }

  if (objectDetected && lastObjectDetected != objectDetected) {
    Serial.println("Objek terdeteksi");
    String warna = identifyColor((int)red, (int)green, (int)blue);
    Serial.println(warna);
    kedip(2, 0.5);
    moveCircle(warna);
    delay(1000);
    myservo.write(200);
    delay(1000);
    myservo.write(0);
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
  if (r > 200 && g < 100 && b < 100) {
    return "Merah";
  } else if (r < 100 && g > 200 && b < 100) {
    return "Hijau";
  } else if (r < 100 && g < 100 && b > 200) {
    return "Biru"; 
  } else if (r > 200 && g > 150 && b > 150) {
    return "Putih";
  } else if (r < 50 && g < 50 && b < 50) {
    return "Hitam";
  } else {
    return "Tidak Dikenal";
  }
}


void moveCircle(String cl) {
  digitalWrite(relayPin, LOW);
  if (cl == "Merah") {
    myStepper.step(step_degree(360));
  } else if (cl == "Hijau") {
    myStepper.step(step_degree(60));
  } else if (cl == "Biru") {
    myStepper.step(step_degree(120));
  } else if (cl == "Putih") {
    myStepper.step(step_degree(180));
  } else if (cl == "Hitam") {
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
