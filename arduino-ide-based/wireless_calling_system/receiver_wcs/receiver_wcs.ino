#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define BUZZER_PIN D5

// Parameter daya sinyal dan faktor pengurangan
const int signalAtOneMeter = -44;    // Nilai RSSI pada jarak 1 meter (dBm)
const float pathLossExponent = 3.0;  // Faktor pengurangan sinyal (n)

const float batasDekat = 1.0;
const float batasJauh = 7.0;

const char *deviceId = "2";

// Ganti dengan SSID dan password dari hotspot yang dipancarkan oleh ESP server
const char *ssid = "WirelessCallerAP";  // SSID dari ESP Server
const char *password = "12345678";                         // Password kosong jika tidak ada

WiFiUDP udp;
const int udpPort = 12345;  // Port tujuan
char incomingPacket[255];
String IPDevice;
IPAddress serverIP;         // Akan diisi setelah terkoneksi

int buzzerStat = 0;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  beep(BUZZER_PIN, 1, 100);
  Serial.begin(115200);

  // Menghubungkan ke hotspot ESP server
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke ");
  Serial.println(ssid);

  int timeout = 20; // timeout 10 detik (20 x 500ms)
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Gagal terhubung ke hotspot.");
    while (1); // Stop program
  }

  beep(BUZZER_PIN, 2, 200);

  Serial.println("\nTerhubung ke Hotspot!");
  IPDevice = WiFi.localIP().toString();
  Serial.print("Alamat IP perangkat ini: ");
  Serial.println(IPDevice);

  // Menyimpan IP server dari gateway (biasanya IP server = gateway)
  serverIP = WiFi.gatewayIP();
  Serial.print("Menggunakan IP server: ");
  Serial.println(serverIP);

  udp.begin(udpPort);
  Serial.println("Memulai UDP...");
}

void loop() {
  long rssi = WiFi.RSSI();
  Serial.print("Nilai RSSI: ");
  Serial.println(rssi);

  float distance = pow(10, (signalAtOneMeter - rssi) / (10 * pathLossExponent));
  Serial.print("Jarak (meter): ");
  Serial.println(distance);

  String message = "Device" + String(deviceId) + "|" + IPDevice + "|" + String(rssi) + "|" + String(distance);

  // Kirim data ke server
  udp.beginPacket(serverIP, udpPort);
  udp.print(message);
  udp.endPacket();

  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
    }
    Serial.print("Data diterima: ");
    Serial.println(incomingPacket);

    if (String(incomingPacket).indexOf("call") > -1 && buzzerStat == 0) {
      buzzerStat = 1;
    }
  }

  if (buzzerStat == 1 && distance <= batasDekat) {
    buzzerStat = 0;
  }

  if (distance >= batasJauh) {
    buzzerStat = 2;
  }

  if (buzzerStat == 2 && distance < batasJauh) {
    buzzerStat = 0;
  }

  if (buzzerStat == 1) {
    beep(BUZZER_PIN, 1, 100);
  } else if (buzzerStat == 2) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(100);
}

void beep(int outpin, int freq, int delayms) {
  for (int i = 0; i < freq; i++) {
    digitalWrite(outpin, HIGH);
    delay(delayms);
    digitalWrite(outpin, LOW);
    delay(delayms);
  }
}
