#include <Wire.h>
#include "Adafruit_TCS34725.h"

// Inisialisasi sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_1X);

// Pin GPIO untuk mengontrol LED
const int ledPin = 15;
const int sensorPin = 34;

float currentClear = 0.0;
float lastClear = 0.0;
float deltaClear = 0.0;
float detectionThreshold = 500.0;

bool setupDone = false;
bool objectDetected = false;
bool lastObjectDetected = false;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);    // Set pin GPIO 15 sebagai output
  digitalWrite(ledPin, LOW);  // Mulai dengan LED mati

  if (tcs.begin()) {
    Serial.println("TCS34725 ditemukan!");
  } else {
    Serial.println("TCS34725 tidak ditemukan. Silakan periksa koneksi.");
    while (1)
      ;  // Stop eksekusi jika sensor tidak ditemukan
  }
}

void loop() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);


  // currentClear = c;
  // deltaClear = currentClear - lastClear;

  // Serial.print("currentClear: ");
  // Serial.print(currentClear);
  // Serial.print(" ");
  // Serial.print("lastClear: ");
  // Serial.print(lastClear);
  // Serial.print(" ");
  // Serial.print("deltaClear: ");
  // Serial.println(deltaClear);

  // Jika nilai clear channel melebihi ambang batas, artinya ada objek di depan sensor
  // if ((abs(deltaClear) >= detectionThreshold && deltaClear > 0) && setupDone) {

  //   objectDetected = true;
  // } else if  ((abs(deltaClear) >= detectionThreshold && deltaClear < 0) && setupDone){
  //    objectDetected = false;
  // }

  int sensorAdc = analogRead(sensorPin);
  Serial.print("ADC: ");
  Serial.println(sensorAdc);

  if(sensorAdc < detectionThreshold){
    objectDetected = true;
  }else{
    objectDetected = false;
  }

  if (objectDetected) {
    digitalWrite(ledPin, HIGH);  // Nyalakan LED
    Serial.println("Objek terdeteksi, LED menyala.");
  }else{
     digitalWrite(ledPin, LOW);  // Matikan LED
    Serial.println("Tidak ada objek, LED mati.");
  }


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

  if (!setupDone) {
    setupDone = true;
  }

  lastClear = currentClear;
  lastObjectDetected = objectDetected;

  delay(500);  // Baca setiap 1 detik
}
