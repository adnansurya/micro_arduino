#define BLYNK_TEMPLATE_ID "TMPL6GsHGgMf0"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "c_cy-OkIWySgf4IXc60_Zt0U65naFriQ"

#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "CustomChars.h"  // Header karakter kustom

#define BLYNK_PRINT Serial
#define OUT_PIN 2

// Konfigurasi pin LoRa
#define SS 5
#define RST 14
#define DIO0 26

// Konfigurasi pin MQ135
const int analogPinMQ135 = 34;     // Pin analog untuk MQ135

// Konfigurasi sensor tegangan
const int analogPinVoltage = 35;        // Pin analog untuk sensor tegangan
const float voltageDividerRatio = 5.0;  // Rasio pembagi tegangan (sesuaikan dengan sensor Anda)

// Konfigurasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Alamat I2C LCD biasanya 0x27

BlynkTimer mainTimer;
char ssid[] = "kontras";   // Ganti dengan nama WiFi Anda
char pass[] = "12345678";  // Ganti dengan password WiFi Anda

float batt_percent, batt2_percent;
int adcValue;

bool isOffline = false;

void setup() {
  pinMode(OUT_PIN, OUTPUT);
  blinkOut(1);

  Serial.begin(115200);
  while (!Serial)
    ;

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Init...");

  // Inisialisasi karakter kustom
  initializeCustomChars(lcd);

  // Inisialisasi LoRa
  Serial.println("Menginisialisasi LoRa...");
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) {  // Frekuensi default 915 MHz
    Serial.println("Gagal menginisialisasi LoRa!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LoRa Error!");
    while (1)
      ;
  }
  Serial.println("LoRa berhasil diinisialisasi!");

  // Mulai koneksi WiFi
  WiFi.begin(ssid, pass);
  Serial.println("Menghubungkan ke WiFi...");

  // Tunggu hingga koneksi berhasil
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Tersambung!");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    isOffline = false;
  } else {
    Serial.println("\nGagal terhubung ke WiFi. Masuk ke mode offline.");
    isOffline = true;
  }

  mainTimer.setInterval(1000L, mainEvent);
  mainTimer.setInterval(10000L, sendData);
  mainTimer.setInterval(100L, receiveLoRaData);  // Cek data masuk dari LoRa
  mainTimer.setInterval(1000L, sendBackup);

  blinkOut(2);
}

void loop() {
  if (!isOffline) {
    Blynk.run();
  }

  mainTimer.run();
}

void mainEvent() {
  // Membaca nilai ADC dari MQ135
  adcValue = analogRead(analogPinMQ135);

  // Membaca tegangan dari sensor tegangan
  float voltage = readVoltage();
  batt_percent = voltage / 4.2 * 100.0;

  // Tampilkan data pada LCD
  displayOnLCD(adcValue, batt_percent, batt2_percent);
}

float readVoltage() {
  int adcValue = analogRead(analogPinVoltage);      // Baca nilai analog
  float sensorVoltage = (adcValue / 4095.0) * 3.3;  // Tegangan pada pin ADC
  return sensorVoltage * voltageDividerRatio;       // Hitung tegangan aktual
}

void displayOnLCD(int adcValue, float percentage, float percentage2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADC: ");
  lcd.print(adcValue);  // Tampilkan nilai ADC

  lcd.setCursor(0, 1);
  lcd.print("B1: ");
  lcd.print(percentage, 0);  // Tampilkan tegangan dengan 2 desimal
  lcd.print("%");
  lcd.setCursor(8, 1);
  lcd.print("B2: ");
  lcd.print(percentage2, 0);  // Tampilkan tegangan dengan 2 desimal
  lcd.print("%");
}

void sendData() {
  if (Blynk.connected()) {
    Serial.println("Koneksi Blynk tersedia, mengirim data ke Blynk...");
    Blynk.virtualWrite(V0, adcValue);
    Blynk.virtualWrite(V1, batt_percent);
    Blynk.virtualWrite(V2, batt2_percent);
    Blynk.virtualWrite(V3, "Normal");
    lcd.setCursor(15, 0);
    lcd.write(byte(0));  // Panah atas
  }
}

void sendBackup() {
  if (!Blynk.connected()) {
    Serial.println("Koneksi Blynk tidak tersedia, mengirim data melalui LoRa...");
    LoRa.beginPacket();
    LoRa.print(adcValue);
    LoRa.print("|");
    LoRa.print(batt_percent);
    LoRa.endPacket();
    lcd.setCursor(15, 0);
    lcd.write(byte(3));  // Panah kanan
    blinkOut(1);
  }
}

void receiveLoRaData() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    lcd.setCursor(15, 0);
    lcd.write(byte(2));  // Panah kiri
    String receivedData = "";
    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }
    Serial.print("Data diterima melalui LoRa: ");
    Serial.println(receivedData);

    batt2_percent = receivedData.toFloat();
  }
}

void blinkOut(int rep) {
  for (int i = 0; i < rep; i++) {
    digitalWrite(OUT_PIN, HIGH);
    delay(200);
    digitalWrite(OUT_PIN, LOW);
    delay(200);
  }
}
