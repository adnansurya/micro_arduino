#include <LoRa.h>

// Konfigurasi pin LoRa
#define SS 5
#define RST 14
#define DIO0 26

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