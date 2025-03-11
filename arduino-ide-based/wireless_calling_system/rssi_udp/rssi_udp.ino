#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>

#define BUZZER_PIN D5

// Parameter daya sinyal dan faktor pengurangan
const int signalAtOneMeter = -40; // Nilai RSSI pada jarak 1 meter (dBm)
const float pathLossExponent = 3.0; // Faktor pengurangan sinyal (n)

const char *ssid_default = "Wireless Caller";


// Konfigurasi UDP
WiFiUDP udp;
const char *udpAddress = "192.168.1.101"; // Ganti dengan alamat IP perangkat penerima
const int udpPort = 12345; // Port tujuan

void setup() {

  pinMode(BUZZER_PIN, OUTPUT);
  beep(BUZZER_PIN, 1, 100);
  Serial.begin(115200);

   // Membuat instance WiFiManager
  WiFiManager wifiManager;

  // Memulai WiFiManager (apabila belum disetel, akan masuk ke mode konfigurasi)
  if (!wifiManager.autoConnect(ssid_default)) {
    Serial.println("Gagal menghubungkan ke WiFi, rebooting...");
    delay(3000);
    ESP.restart();
  }

  beep(BUZZER_PIN, 2, 200);
  
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
  String message = "Device2;RSSI:" + String(rssi) + ";Distance:" + String(distance);

  // Mengirim pesan melalui UDP
  udp.beginPacket(udpAddress, udpPort);
  udp.print(message);
  udp.endPacket();

  Serial.println("Data dikirim melalui UDP!");

  delay(1000); // Mengirimkan data setiap detik
}


void beep(int outpin, int freq, int delayms) {
  for (int i = 0; i < freq; i++) {
    digitalWrite(outpin, HIGH);
    delay(delayms);
    digitalWrite(outpin, LOW);
    delay(delayms);
  }
}
