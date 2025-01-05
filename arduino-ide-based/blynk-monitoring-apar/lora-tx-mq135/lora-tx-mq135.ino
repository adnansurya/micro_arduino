#include <LoRa.h>

// Konfigurasi pin LoRa
#define SS 5
#define RST 14
#define DIO0 26

// Konfigurasi pin MQ135
const int analogPin = 34; // Pin analog untuk MQ135
const float RL = 10.0;    // Resistor load dalam kilo-ohm
const float VCC = 3.3;    // Tegangan catu daya ESP32
const float ratioCleanAir = 3.62; // Rasio RS/R0 di udara bersih

float R0 = 1.0; // Nilai referensi R0 (lakukan kalibrasi sebelumnya)

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Inisialisasi LoRa
  Serial.println("Menginisialisasi LoRa...");
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) { // Frekuensi default 915 MHz, sesuaikan dengan modul LoRa Anda
    Serial.println("Gagal menginisialisasi LoRa!");
    while (1);
  }
  Serial.println("LoRa berhasil diinisialisasi!");

  // Kalibrasi sensor MQ135
  Serial.println("Kalibrasi sensor...");
  R0 = calibrateSensor();
  Serial.print("Nilai R0: ");
  Serial.println(R0);
}

void loop() {
  // Membaca data PPM dari MQ135
  float rs = readSensor();
  float ratio = rs / R0;
  float ppm = pow(10, ((log10(ratio) - (-0.45)) / (-0.32))); // m dan b dari datasheet
  ppm = 9.12;

  // Kirim data melalui LoRa
  Serial.print("Mengirim PPM CO: ");
  Serial.println(ppm);

  LoRa.beginPacket();
  LoRa.print(ppm); // Kirim data PPM sebagai string
  LoRa.endPacket();

  delay(2000); // Delay 2 detik sebelum pengukuran berikutnya
}

float readSensor() {
  int adcValue = analogRead(analogPin);        // Baca nilai analog
  float sensorVoltage = (adcValue / 4095.0) * VCC; // Konversi ke tegangan (12-bit ADC)
  float rs = ((VCC - sensorVoltage) / sensorVoltage) * RL; // Hitung RS
  return rs;
}

float calibrateSensor() {
  float totalRS = 0;
  for (int i = 0; i < 100; i++) { // Ambil rata-rata dari 100 pembacaan
    totalRS += readSensor();
    delay(50);
  }
  float avgRS = totalRS / 100.0;
  return avgRS / ratioCleanAir; // Hitung R0 berdasarkan rasio udara bersih
}
