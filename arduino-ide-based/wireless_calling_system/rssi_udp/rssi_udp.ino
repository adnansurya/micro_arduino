#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>

#define BUZZER_PIN D5

// Parameter daya sinyal dan faktor pengurangan
const int signalAtOneMeter = -44;    // Nilai RSSI pada jarak 1 meter (dBm)
const float pathLossExponent = 3.0;  // Faktor pengurangan sinyal (n)

const float batasDekat = 1.0;
const float batasJauh = 7.0;


const char *deviceId = "2";
const char *ssid_default = "Wireless Caller 2";


// Konfigurasi UDP
WiFiUDP udp;
const char *udpAddress = "192.168.164.140";  // Ganti dengan alamat IP perangkat penerima
const int udpPort = 12345;                   // Port tujuan
String IPDevice;
char incomingPacket[255];

int buzzerStat = 0;

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
  IPDevice = WiFi.localIP().toString();
  Serial.println(IPDevice);

  // Memulai koneksi UDP
  udp.begin(udpPort);
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
  String message = "Device" + String(deviceId) + "|" + IPDevice + "|" + String(rssi) + "|" + String(distance);

  // Mengirim pesan melalui UDP
  udp.beginPacket(udpAddress, udpPort);
  udp.print(message);
  udp.endPacket();

  // Serial.println("Data dikirim melalui UDP!");

  int packetSize = udp.parsePacket();  // Mengecek apakah ada paket yang diterima
  if (packetSize) {
    // Membaca data dari paket
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';  // Menambahkan null terminator ke string
    }
    
    Serial.print("Data diterima: ");
    Serial.println(incomingPacket);    

    if(String(incomingPacket).indexOf("call") > -1 && buzzerStat == 0){
      buzzerStat = 1;
    }
  }

  if(buzzerStat == 1 && distance <= batasDekat){
    buzzerStat = 0;
  } 

  if(distance >= batasJauh){
    buzzerStat = 2;
  } 

  if(buzzerStat == 2 && distance < batasJauh){
    buzzerStat = 0; 
  }

  if (buzzerStat == 1) {
    beep(BUZZER_PIN, 1, 100);
  }else if(buzzerStat == 2){
    digitalWrite(BUZZER_PIN, HIGH);
  }else{
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(100);  // Mengirimkan data setiap detik
}


void beep(int outpin, int freq, int delayms) {
  for (int i = 0; i < freq; i++) {
    digitalWrite(outpin, HIGH);
    delay(delayms);
    digitalWrite(outpin, LOW);
    delay(delayms);
  }
}
