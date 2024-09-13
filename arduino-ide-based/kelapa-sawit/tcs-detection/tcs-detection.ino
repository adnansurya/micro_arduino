#include <Wire.h>
#include "Adafruit_TCS34725.h"

// Inisialisasi sensor dengan waktu integrasi dan gain
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_1X);

// Ambang batas deteksi objek
const uint16_t detectionThreshold = 50;

// Pin GPIO untuk mengontrol LED
const int ledPin = 15;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);  // Set pin GPIO 15 sebagai output
  digitalWrite(ledPin, LOW); // Mulai dengan LED mati

  if (tcs.begin()) {
    Serial.println("TCS34725 ditemukan!");
  } else {
    Serial.println("TCS34725 tidak ditemukan. Silakan periksa koneksi.");
    while (1); // Stop eksekusi jika sensor tidak ditemukan
  }
}

void loop() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  Serial.print("Clear: "); Serial.print(c); Serial.print(" ");
  Serial.print("R: "); Serial.print(r); Serial.print(" ");
  Serial.print("G: "); Serial.print(g); Serial.print(" ");
  Serial.print("B: "); Serial.println(b);

  // Jika nilai clear channel melebihi ambang batas, artinya ada objek di depan sensor
  if (c > detectionThreshold) {
    digitalWrite(ledPin, HIGH); // Nyalakan LED
    Serial.println("Objek terdeteksi, LED menyala.");
  } else {
    digitalWrite(ledPin, LOW); // Matikan LED
    Serial.println("Tidak ada objek, LED mati.");
  }

  delay(500); // Tunggu 0.5 detik sebelum membaca lagi
}
