#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Stepper.h>

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

// Pin GPIO untuk mengontrol LED
const int ledPin = 15;

float currentClear = 0.0;
float detectionThreshold = 10.0;

bool objectDetected = false;
bool lastObjectDetected = false;

uint16_t r, g, b, c;




void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);     // Set pin GPIO 15 sebagai output
  digitalWrite(ledPin, HIGH);  // Mulai dengan LED mati

  if (tcs.begin()) {
    Serial.println("TCS34725 ditemukan!");
  } else {
    Serial.println("TCS34725 tidak ditemukan. Silakan periksa koneksi.");
    while (1)
      ;  // Stop eksekusi jika sensor tidak ditemukan
  }

  myStepper.setSpeed(8);
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
    String warna = classify();
    Serial.println(warna);
    kedip(2, 0.5);
    moveCircle(warna);
    
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


String classify() {

  String result = "-";
  // Konversi nilai raw ke format RGB
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

  if (red > green && red > blue) {
    result = "red";
  } else if (green > red && green > blue) {
    result = "green";
  } else if (blue > red && blue > green) {
    result = "blue";
  } else {
    result = "unknown";
  }

  return result;
}


void moveCircle(String cl) {
  if (cl == "unknown") {
    myStepper.step(step_degree(360));
  } else if (cl == "red") {
    myStepper.step(step_degree(90));
  } else if (cl == "green") {
    myStepper.step(step_degree(180));
  } else if (cl == "blue") {
    myStepper.step(step_degree(270));
  }
}


int step_degree(float desired_degree) {
  Serial.print("CURRENT : ");
  Serial.println(currentAngle);
  int step_moved = (desired_degree - currentAngle) / resolution;
  currentAngle = desired_degree;
  return step_moved;
}
