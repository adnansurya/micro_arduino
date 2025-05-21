#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Konfigurasi tombol
#define button1 D5
#define button2 D6

// Konfigurasi LCD 20x4 (alamat I2C: 0x27, sesuaikan dengan modul Anda)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Konfigurasi UDP
WiFiUDP udp;
const int udpPort = 12345;
char incomingPacket[255];

// Variabel untuk menyimpan data
String device1Data = "Menunggu...";
String device2Data = "Menunggu...";

const char* ap_ssid = "WirelessCallerAP";
const char* ap_password = "12345678";  // Minimal 8 karakter

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
  lcd.print("Menyalakan AP...");

  // Konfigurasi tombol
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);

  // Memulai Access Point
  WiFi.softAP(ap_ssid, ap_password);
  delay(1000); // Tunggu AP aktif
  IPAddress = WiFi.softAPIP().toString();

  Serial.println("AP Mode aktif");
  Serial.print("SSID: "); Serial.println(ap_ssid);
  Serial.print("Password: "); Serial.println(ap_password);
  Serial.print("IP Address: "); Serial.println(IPAddress);

  // Memulai UDP
  udp.begin(udpPort);
  Serial.print("Listening UDP on port ");
  Serial.println(udpPort);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menunggu data...");
}

void loop() {
  if (millis() > (lcdSeconds * 1000) + lastLcdMillis) {
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

  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
    }
    Serial.print("Data diterima: ");
    Serial.println(incomingPacket);

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

  delay(100);
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
  const char *ipSend = ipStr.c_str();
  const char *msgSend = msgStr.c_str();

  udp.beginPacket(ipSend, udpPort);
  udp.write(msgSend);
  udp.endPacket();

  Serial.print("Pesan dikirim ke ");
  Serial.print(ipSend);
  Serial.print(": ");
  Serial.println(msgSend);
}
