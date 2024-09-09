#include <Wire.h>
#include "Adafruit_TCS34725.h"

// Inisialisasi sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_1X);

// Pin GPIO untuk mengontrol LED
const int ledPin = 15;

float currentClear = 0.0;
float detectionThreshold = 10.0;

bool objectDetected = false;
bool lastObjectDetected = false;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);    // Set pin GPIO 15 sebagai output
  digitalWrite(ledPin, HIGH);  // Mulai dengan LED mati

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


  currentClear = c;

  Serial.print("currentClear: ");
  Serial.println(currentClear);
 

  if(currentClear > detectionThreshold){
    objectDetected = true;
  }else{
    objectDetected = false;
  }

  if (objectDetected && lastObjectDetected != objectDetected) {
    Serial.println("Objek terdeteksi");
    kedip(2, 0.5);
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

  lastObjectDetected = objectDetected;

  delay(500);  // Baca setiap 1 detik
}

void kedip(int ulang, float detik) {
  for (int i = 0; i < ulang; i++) {
    digitalWrite(ledPin, LOW);
    delay(detik * 1000);
    digitalWrite(ledPin, HIGH);
    delay(detik * 1000);
  }
}
