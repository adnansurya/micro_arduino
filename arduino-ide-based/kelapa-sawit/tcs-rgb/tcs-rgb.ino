#include <Wire.h>
#include "Adafruit_TCS34725.h"

// Inisialisasi sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_1X);

void setup() {
  Serial.begin(115200);

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

  Serial.print("R: "); Serial.print((int)red); Serial.print(" ");
  Serial.print("G: "); Serial.print((int)green); Serial.print(" ");
  Serial.print("B: "); Serial.println((int)blue);

  delay(1000); // Baca setiap 1 detik
}
