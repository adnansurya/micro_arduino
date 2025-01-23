#define BLYNK_TEMPLATE_ID "TMPL6GsHGgMf0"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "c_cy-OkIWySgf4IXc60_Zt0U65naFriQ"

#include <LoRa.h>
#include "esp_system.h"  // Library untuk esp_restart()
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define BLYNK_PRINT Serial
#define OUT_PIN 2
#define BUZZER_PIN 15


// Konfigurasi pin LoRa
#define SS 5
#define RST 14
#define DIO0 26

// Konfigurasi sensor tegangan
const int analogPinVoltage = 35;        // Pin analog untuk sensor tegangan
const float voltageDividerRatio = 5.0;  // Rasio pembagi tegangan (sesuaikan dengan sensor Anda)
const float VCC = 3.3;

float voltage2, batt_percent, batt2_percent;
int adcValue;
String status, lastStatus;
int statCheckSeconds = 20;
bool backupMode = false;

BlynkTimer mainTimer;
char ssid[] = "apar_backup";  // Ganti dengan nama WiFi Anda
char pass[] = "12345678";     // Ganti dengan password WiFi Anda

unsigned long loraTimeout = 30000;    // Waktu tunggu dalam milidetik (5 detik)
unsigned long lastLoraInputTime = 0;  // Waktu terakhir input diterima

void setup() {
  pinMode(OUT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  blinkOut(1);
  Serial.begin(115200);
  while (!Serial)
    ;

  // Inisialisasi LoRa
  Serial.println("Menginisialisasi LoRa...");
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) {  // Frekuensi default 915 MHz
    Serial.println("Gagal menginisialisasi LoRa!");
    while (1)
      ;
  }
  Serial.println("LoRa berhasil diinisialisasi!");



  Serial.print("Menunggu paket dari LoRa");
  // delay(100);
  for (int i = 0; i < statCheckSeconds * 10; i++) {
    if (i % 10 == 0) {
      Serial.print(".");
    }
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      Serial.print("Paket diterima: ");
      String received = "";
      while (LoRa.available()) {
        received += (char)LoRa.read();
      }
      Serial.println(received);
      backupMode = true;
      break;
    }
    delay(100);
  }

  if (backupMode) {
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    mainTimer.setInterval(10000L, sendData);
    Serial.println("Blynk berhasil diinisialisasi.");
  }

  mainTimer.setInterval(1000L, mainEvent);
  mainTimer.setInterval(100L, receiveLoRaData);  // Cek data masuk dari LoRa

  blinkOut(2);
}

void loop() {


  if (backupMode) {
    Blynk.run();
  }
  mainTimer.run();
}

float readVoltage() {
  int adcValue = analogRead(analogPinVoltage);      // Baca nilai analog
  float sensorVoltage = (adcValue / 4095.0) * VCC;  // Tegangan pada pin ADC
  return sensorVoltage * voltageDividerRatio;       // Hitung tegangan aktual
}

void blinkOut(int rep) {
  for (int i = 0; i < rep; i++) {
    digitalWrite(OUT_PIN, HIGH);
    delay(200);
    digitalWrite(OUT_PIN, LOW);
    delay(200);
  }
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

void mainEvent() {
  voltage2 = readVoltage();
  batt2_percent = voltage2 / 4.2 * 100.0;

  Serial.print("Tegangan yang diukur: ");
  Serial.print(voltage2);
  Serial.print(" V\t");
  Serial.print(batt2_percent);
  Serial.println(" %");

  if (status == "Bocor") {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Kirim data tegangan melalui LoRa
  if (!backupMode) {
    LoRa.beginPacket();
    LoRa.print(batt2_percent, 2);  // Kirim dengan 2 desimal
    LoRa.endPacket();
  }

  if (Blynk.connected() && status != lastStatus && status == "Bocor") {
    Blynk.logEvent("peringatan", "Terjadi Kebocoran!");
  }


  lastStatus = status
}

void receiveLoRaData() {
  // Cek apakah ada data yang diterima
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    if (backupMode) {
      Serial.print("Paket diterima: ");
      String received = "";
      while (LoRa.available()) {
        received += (char)LoRa.read();
      }
      Serial.println(received);

      // Konversi string ke float jika diperlukan
      adcValue = getValue(received, '|', 0).toInt();
      batt_percent = getValue(received, '|', 1).toFloat();
      status = getValue(received, '|', 2);
      Serial.print("ADC: ");
      Serial.println(adcValue);
      Serial.print("Batt Percent: ");
      Serial.println(batt_percent);
      Serial.print("Status: ");
      Serial.println(status);
      lastLoraInputTime = millis();
      blinkOut(1);
    } else {
      Serial.println("Data dari LoRa diterima. Masuk ke mode Backup");
      Serial.println("RESTART");
      delay(2000);
      esp_restart();
    }
  } else {
    if (backupMode && millis() - lastLoraInputTime > loraTimeout) {
      Serial.println("Timeout mode backup tercapai. Kembali ke mode Normal");
      Serial.println("RESTART");
      esp_restart();
    }
  }
}


void sendData() {
  Blynk.virtualWrite(V0, adcValue);
  Blynk.virtualWrite(V1, batt_percent);
  Blynk.virtualWrite(V2, batt2_percent);
  Blynk.virtualWrite(V3, "Backup");
  Blynk.virtualWrite(V4, status);
}
