#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Konfigurasi LCD 20x4 (alamat I2C: 0x27, sesuaikan dengan modul Anda)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Konfigurasi UDP
WiFiUDP udp;
const int udpPort = 12345; // Port untuk menerima data
char incomingPacket[255]; // Buffer untuk menyimpan paket yang diterima

// Variabel untuk menyimpan data masing-masing perangkat
String device1Data = "Menunggu...";
String device2Data = "Menunggu...";

void setup() {
  Serial.begin(115200);

  // Menyambungkan ke WiFi
  WiFi.begin("SSID_WIFI", "PASSWORD_WIFI"); // Ganti dengan SSID dan password WiFi Anda
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTerhubung ke WiFi");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  // Memulai koneksi UDP
  udp.begin(udpPort);
  Serial.print("Listening UDP on port ");
  Serial.println(udpPort);

  // Inisialisasi LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Menunggu data...");
}

void loop() {
  int packetSize = udp.parsePacket(); // Mengecek apakah ada paket yang diterima
  if (packetSize) {
    // Membaca data dari paket
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0'; // Menambahkan null terminator ke string
    }
    Serial.print("Data diterima: ");
    Serial.println(incomingPacket);

    // Memproses data berdasarkan identitas perangkat
    String data = String(incomingPacket);
    if (data.startsWith("Device1")) {
      device1Data = data;
    } else if (data.startsWith("Device2")) {
      device2Data = data;
    }

    // Menampilkan data pada LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Device 1:");
    lcd.setCursor(0, 1);
    lcd.print(device1Data);
    lcd.setCursor(0, 2);
    lcd.print("Device 2:");
    lcd.setCursor(0, 3);
    lcd.print(device2Data);
  }

  delay(100); // Mengurangi beban prosesor
}
