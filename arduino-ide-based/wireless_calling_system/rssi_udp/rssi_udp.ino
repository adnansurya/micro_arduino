#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

// Parameter daya sinyal dan faktor pengurangan
const int signalAtOneMeter = -40; // Nilai RSSI pada jarak 1 meter (dBm)
const float pathLossExponent = 3.0; // Faktor pengurangan sinyal (n)

// Konfigurasi UDP
WiFiUDP udp;
const char *udpAddress = "192.168.1.100"; // Ganti dengan alamat IP tujuan
const int udpPort = 12345; // Port tujuan

void setup() {
  Serial.begin(115200);

  // Membuat instance WiFiManager
  WiFiManager wifiManager;

  // Memulai WiFiManager (apabila belum disetel, akan masuk ke mode konfigurasi)
  if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("Gagal menghubungkan ke WiFi, rebooting...");
    delay(3000);
    ESP.restart();
  }

  // Terkoneksi ke jaringan WiFi
  Serial.println("Terhubung ke WiFi!");

  // Memulai koneksi UDP
  udp.begin(udpPort);
  Serial.println("UDP dimulai");
}

void loop() {
  // Mengukur RSSI
  long rssi = WiFi.RSSI();
  Serial.print("Nilai RSSI: ");
  Serial.println(rssi);

  // Menghitung jarak berdasarkan nilai RSSI
  float distance = pow(10, (signalAtOneMeter - rssi) / (10 * pathLossExponent));
  Serial.print("Jarak (meter): ");
  Serial.println(distance);

  // Menyiapkan pesan untuk dikirimkan
  String message = "RSSI: " + String(rssi) + " dBm, Jarak: " + String(distance) + " meter";

  // Mengirim pesan melalui UDP
  udp.beginPacket(udpAddress, udpPort);
  udp.print(message);
  udp.endPacket();

  Serial.println("Data dikirimkan melalui UDP!");

  delay(1000); // Mengukur dan mengirim data setiap detik
}
