#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Parameter daya sinyal dan faktor pengurangan
const int signalAtOneMeter = -40; // Nilai RSSI pada jarak 1 meter (dBm)
const float pathLossExponent = 3.0; // Faktor pengurangan sinyal (n)

// Konfigurasi WiFi
const char *ssid = "SSID_WIFI";        // Ganti dengan SSID jaringan WiFi Anda
const char *password = "PASSWORD_WIFI"; // Ganti dengan password WiFi Anda

// Konfigurasi UDP
WiFiUDP udp;
const char *udpAddress = "192.168.1.101"; // Ganti dengan alamat IP perangkat penerima
const int udpPort = 12345; // Port tujuan

void setup() {
  Serial.begin(115200);

  // Menyambungkan ke WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTerhubung ke WiFi");
  Serial.print("Alamat IP Pengirim: ");
  Serial.println(WiFi.localIP());

  // Memulai koneksi UDP
  Serial.println("Memulai UDP...");
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
  String message = "Device1;RSSI:" + String(rssi) + ";Distance:" + String(distance);

  // Mengirim pesan melalui UDP
  udp.beginPacket(udpAddress, udpPort);
  udp.print(message);
  udp.endPacket();

  Serial.println("Data dikirim melalui UDP!");

  delay(1000); // Mengirimkan data setiap detik
}
