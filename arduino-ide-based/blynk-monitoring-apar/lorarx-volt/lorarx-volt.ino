#include <LoRa.h>

// Konfigurasi pin LoRa
#define SS 5
#define RST 14
#define DIO0 26

// Konfigurasi sensor tegangan
const int analogPinVoltage = 35;        // Pin analog untuk sensor tegangan
const float voltageDividerRatio = 5.0;  // Rasio pembagi tegangan (sesuaikan dengan sensor Anda)
const float VCC = 3.3;  

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Inisialisasi LoRa
  Serial.println("Menginisialisasi LoRa...");
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) { // Frekuensi default 915 MHz
    Serial.println("Gagal menginisialisasi LoRa!");
    while (1);
  }
  Serial.println("LoRa berhasil diinisialisasi!");


}

void loop() {

  // Konversi nilai ADC ke tegangan dalam volt (asumsikan input ADC 3.3V dan 12-bit resolusi)
  float voltage = readVoltage();

  Serial.print("Tegangan yang diukur: ");
  Serial.print(voltage);
  Serial.println(" V");

  // Kirim data tegangan melalui LoRa
  LoRa.beginPacket();
  LoRa.print(voltage, 2); // Kirim dengan 2 desimal
  LoRa.endPacket();

  delay(1000); // Interval 1 detik

  // Cek apakah ada data yang diterima
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Paket diterima: ");
    String received = "";
    while (LoRa.available()) {
      received += (char)LoRa.read();
    }
    Serial.println(received);

    // Konversi string ke float jika diperlukan
    float ppm = received.toFloat();
    Serial.print("Nilai PPM CO: ");
    Serial.println(ppm);
  }
}

float readVoltage() {
  int adcValue = analogRead(analogPinVoltage);      // Baca nilai analog
  float sensorVoltage = (adcValue / 4095.0) * VCC;  // Tegangan pada pin ADC
  return sensorVoltage * voltageDividerRatio;       // Hitung tegangan aktual
}
