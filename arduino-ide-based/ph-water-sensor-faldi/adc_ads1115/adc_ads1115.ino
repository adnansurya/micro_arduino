#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// Inisialisasi objek ADS1115
Adafruit_ADS1115 ads;  

void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1); // SDA = D2 (GPIO4), SCL = D1 (GPIO5)

  if (!ads.begin()) {
    Serial.println("Gagal mendeteksi ADS1115. Periksa koneksi!");
    while (1);
  }

  Serial.println("ADS1115 Terdeteksi!");
  // Set gain jika perlu (default 2/3x = ±6.144V)
  ads.setGain(GAIN_TWOTHIRDS);  
}

void loop() {
  int16_t adc0 = ads.readADC_SingleEnded(0);  // Baca channel 0
  float voltage0 = ads.computeVolts(adc0);    // Konversi ke tegangan

  Serial.print("ADC0: ");
  Serial.print(adc0);
  Serial.print("\tTegangan: ");
  Serial.print(voltage0, 4);
  Serial.println(" V");

  delay(1000);  // Tunggu 1 detik
}
