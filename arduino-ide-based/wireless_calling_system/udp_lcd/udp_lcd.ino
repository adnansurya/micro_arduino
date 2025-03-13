#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>

// Konfigurasi tombol
#define button1 D5
#define button2 D6

// Konfigurasi LCD 20x4 (alamat I2C: 0x27, sesuaikan dengan modul Anda)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Konfigurasi UDP
WiFiUDP udp;
const int udpPort = 12345;  // Port untuk menerima data
char incomingPacket[255];   // Buffer untuk menyimpan paket yang diterima

// Variabel untuk menyimpan data masing-masing perangkat
String device1Data = "Menunggu...";
String device2Data = "Menunggu...";

const char* ssid_default = "Wireless Caller Transmitter";

String IPAddress;

unsigned long lastLcdMillis = 0;
unsigned long lcdSeconds = 1;

int rssi1, rssi2;
float jarak1, jarak2;

String deviceIP1, deviceIP2;

void setup() {
  Serial.begin(115200);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  // Konfigurasi tombol sebagai input
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);

  // Menyambungkan ke WiFi
  // Membuat instance WiFiManager
  WiFiManager wifiManager;

  // Memulai WiFiManager (apabila belum disetel, akan masuk ke mode konfigurasi)
  if (!wifiManager.autoConnect(ssid_default)) {
    Serial.println("Gagal menghubungkan ke WiFi, rebooting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("\nTerhubung ke WiFi");
  Serial.print("Alamat IP: ");
  IPAddress = WiFi.localIP().toString();
  Serial.println();

  // Memulai koneksi UDP
  udp.begin(udpPort);
  Serial.print("Listening UDP on port ");
  Serial.println(udpPort);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menunggu data...");
}

void loop() {

  if (millis() > (lcdSeconds * 1000) + lastLcdMillis) {

    // Menampilkan data pada LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP: ");
    lcd.print(IPAddress);
    lcd.setCursor(0, 2);
    lcd.print("D1: ");
    lcd.print(rssi1);
    lcd.print(" ");
    lcd.print(jarak1);
    lcd.setCursor(0, 3);
    lcd.print("D2: ");
    lcd.print(rssi2);
    lcd.print(" ");
    lcd.print(jarak2);
    lastLcdMillis = millis();
  }

  // Cek apakah tombol ditekan
  if (digitalRead(button1) == LOW) {
    Serial.println("Call Device 1");
    sendUDPMessage(deviceIP1, "call");
    delay(500);
  }
  if (digitalRead(button2) == LOW) {
    Serial.println("Call Device 2");
    sendUDPMessage(deviceIP2, "call");
    delay(500);
  }



  int packetSize = udp.parsePacket();  // Mengecek apakah ada paket yang diterima
  if (packetSize) {
    // Membaca data dari paket
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';  // Menambahkan null terminator ke string
    }
    Serial.print("Data diterima: ");
    Serial.println(incomingPacket);

    // Memproses data berdasarkan identitas perangkat
    String data = String(incomingPacket);
    if (data.startsWith("Device1")) {
      device1Data = data;
      deviceIP1 = getValue(device1Data, '|', 1);
      rssi1 = getValue(device1Data, '|', 2).toInt();
      jarak1 = getValue(device1Data, '|', 3).toFloat();
    } else if (data.startsWith("Device2")) {
      device2Data = data;
      deviceIP2 = getValue(device2Data, '|', 1);
      rssi2 = getValue(device2Data, '|', 2).toInt();
      jarak2 = getValue(device2Data, '|', 3).toFloat();
    }
  }

  delay(100);  // Mengurangi beban prosesor
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}


void sendUDPMessage(String ipStr, String msgStr) {
  const char *ipSend = ipStr.c_str();  // Konversi String ke const char*
  const char *msgSend = msgStr.c_str();  // Konversi String ke const char*

  udp.beginPacket(ipSend, udpPort);
  udp.write(msgSend);
  udp.endPacket();

  Serial.print("Pesan dikirim ke ");
  Serial.print(ipSend);  // Gunakan ipSend yang sudah dikonversi
  Serial.print(": ");
  Serial.println(msgSend);
}
