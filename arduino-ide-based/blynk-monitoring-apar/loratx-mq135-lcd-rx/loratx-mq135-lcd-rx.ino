#define BLYNK_TEMPLATE_ID "TMPL6GsHGgMf0"
#define BLYNK_TEMPLATE_NAME "Quickstart Template"
#define BLYNK_AUTH_TOKEN "c_cy-OkIWySgf4IXc60_Zt0U65naFriQ"

#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#define BLYNK_PRINT Serial
#define OUT_PIN 2

// Konfigurasi pin LoRa
#define SS 5
#define RST 14
#define DIO0 26

// Konfigurasi pin MQ135
const int analogPinMQ135 = 34;     // Pin analog untuk MQ135
const float RL = 10.0;             // Resistor load dalam kilo-ohm
const float VCC = 3.3;             // Tegangan catu daya ESP32
const float ratioCleanAir = 3.62;  // Rasio RS/R0 di udara bersih

float R0 = 1.0;  // Nilai referensi R0 (lakukan kalibrasi sebelumnya)

// Konfigurasi sensor tegangan
const int analogPinVoltage = 35;        // Pin analog untuk sensor tegangan
const float voltageDividerRatio = 5.0;  // Rasio pembagi tegangan (sesuaikan dengan sensor Anda)

// Konfigurasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Alamat I2C LCD biasanya 0x27

BlynkTimer mainTimer;
char ssid[] = "kontras";   // Ganti dengan nama WiFi Anda
char pass[] = "12345678";  // Ganti dengan password WiFi Anda

float batt_percent, batt2_percent;
float ppm;

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

  // Kalibrasi sensor MQ135
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Kalibrasi...");
  Serial.println("Kalibrasi sensor...");
  R0 = calibrateSensor();
  Serial.print("Nilai R0: ");
  Serial.println(R0);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("R0: ");
  lcd.setCursor(5, 0);
  lcd.print(R0, 2);  // Tampilkan nilai R0 dengan 2 desimal

  Serial.println("Semua sensor berhasil diinisialisasi.");

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
  mainTimer.setInterval(500L, receiveLoRaData);  // Cek data masuk dari LoRa
  mainTimer.setInterval(1000L, sendBackup);

  blinkOut(2);
}

void loop() {

  if(!isOffline){
     Blynk.run();
  }
 
  mainTimer.run();
}

void mainEvent() {
  // Membaca data PPM dari MQ135
  float rs = readSensorMQ135();
  float ratio = rs / R0;
  ppm = pow(10, ((log10(ratio) - (-0.45)) / (-0.32)));  // m dan b dari datasheet

  // Membaca tegangan dari sensor tegangan
  float voltage = readVoltage();
  batt_percent = voltage / 4.2 * 100.0;

  // Tampilkan data pada LCD
  displayOnLCD(ppm, batt_percent, batt2_percent);
}

float readSensorMQ135() {
  int adcValue = analogRead(analogPinMQ135);
  float sensorVoltage = (adcValue / 4095.0) * VCC;          // Konversi ke tegangan (12-bit ADC)
  float rs = ((VCC - sensorVoltage) / sensorVoltage) * RL;  // Hitung RS
  return rs;
}

float calibrateSensor() {
  float totalRS = 0;
  for (int i = 0; i < 100; i++) {  // Ambil rata-rata dari 100 pembacaan
    totalRS += readSensorMQ135();
    delay(50);
  }
  float avgRS = totalRS / 100.0;
  return avgRS / ratioCleanAir;  // Hitung R0 berdasarkan rasio udara bersih
}

float readVoltage() {
  int adcValue = analogRead(analogPinVoltage);      // Baca nilai analog
  float sensorVoltage = (adcValue / 4095.0) * VCC;  // Tegangan pada pin ADC
  return sensorVoltage * voltageDividerRatio;       // Hitung tegangan aktual
}

void displayOnLCD(float ppm, float percentage, float percentage2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO : ");
  lcd.print(ppm, 2);  // Tampilkan nilai PPM dengan 2 desimal
  lcd.print(" ppm");

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
    Blynk.virtualWrite(V0, ppm);
    Blynk.virtualWrite(V1, batt_percent);
    Blynk.virtualWrite(V2, batt2_percent);
  } 
}

void sendBackup(){
  if(!Blynk.connected()) {
    Serial.println("Koneksi Blynk tidak tersedia, mengirim data melalui LoRa...");
    LoRa.beginPacket();
    LoRa.print(ppm);
    LoRa.print("|");
    LoRa.print(batt_percent);
    LoRa.endPacket();
    blinkOut(1);
  }
}

void receiveLoRaData() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
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
