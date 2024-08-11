#include <ThingerESP32.h>
#include <WiFi.h>
#include "ACS712.h"
#include <ZMPT101B.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SENSITIVITY 500.0f

#define USERNAME "MuchlisBaso"
#define DEVICE_ID "Bismillah18"
#define DEVICE_CREDENTIAL "esp32Panel"  // jangan diubah2

#define SSID "MIKRO"
#define SSID_PASSWORD "IDEAlist"

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

const int ledPin = 2;
const int sensorPin1 = 32;
const int sensorPin2 = 33;  // Pin analog yang digunakan untuk membaca sensor


float Current;  // Variabel untuk menyimpan nilai arus
float Voltage;  // variable untuk menyimpan nilai tegangan
float Power;
float energy_kWh = 0.0;  // Variabel untuk menyimpan energi dalam kWh

String baris1, baris2;

const unsigned long periodeKirim = 60000;  //60 detik (milisekon)
unsigned long waktuSekarang = 0;
unsigned long waktuKirimSebelumnya = 0;

ACS712 sensor(ACS712_30A, sensorPin2);
ZMPT101B voltageSensor(sensorPin1, 50.0);
LiquidCrystal_I2C lcd(0x27, 16, 2);


bool tampilDaya = false;
bool systemReady = false;
const unsigned long periodeDisplay = 3000;  //periode ganti tampilan (milisekon)
unsigned long waktuDisplaySebelumnya = 0;



void setup() {
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();

  // open serial for monitoring
  Serial.begin(115200);
  // set builtin led as output
  pinMode(ledPin, OUTPUT);

  // add WiFi credentials
  displayLCD("Starting...", "Wifi Connect");
  thing.add_wifi(SSID, SSID_PASSWORD);

  digitalWrite(ledPin, HIGH);  // Hidupkan LED
  delay(1000);                 // Tunggu 1 detik
  digitalWrite(ledPin, LOW);   // Matikan LED
  delay(1000);                 // Tunggu 1 detik

  displayLCD("Lepaskan", "Beban");
  delay(2000);
  displayLCD("Sebelum", "Pengukuran");
  delay(2000);

  displayLCD("Callibrating", "Sensors...");
  sensor.calibrate();
  voltageSensor.setSensitivity(SENSITIVITY);

  thing["HasilUkur"] >> [](pson& out) {
    // int sensorValue1 = analogRead(sensorPin1);
    // Voltage = sensorValue1 * (3, 3 / 4095.0);
    Voltage = voltageSensor.getRmsVoltage();
    Serial.print("Voltage: ");
    Serial.print(Voltage);
    Serial.println(" V");
    out["voltage"] = Voltage;

    // int sensorValue2 = analogRead(sensorPin2);
    // Current = (sensorValue2 - 2047.5) / 185.0;
    Current = sensor.getCurrentAC();
    Serial.print("Current: ");
    Serial.print(Current);
    Serial.println(" A");
    Serial.println();
    out["current"] = Current;

    Power = Voltage * Current;
    out["power"] = Power;

    // Menghitung energi yang digunakan per menit
    energy_kWh = energy_kWh + (Power / 60000.0);
    out["kWh"] = energy_kWh;

    if (Power > 0.0 && !systemReady) {
      systemReady = true;
      displayLCD("System", "Ready!");
    }
  };
}

void loop() {
  //variabke untuk menghandle program ke thinger.io
  thing.handle();

  waktuSekarang = millis();
  if (waktuSekarang - waktuKirimSebelumnya >= periodeKirim) {

    thing.write_bucket("hasil_ukur", "HasilUkur");
    waktuKirimSebelumnya = waktuSekarang;
  }

  if (waktuSekarang - waktuDisplaySebelumnya >= periodeDisplay && systemReady) {

    if (tampilDaya) {
      baris1 = "P = " + String(Power) + " Watt";
      baris2 = "W = " + String(energy_kWh) + " kWh";
    } else {
      baris1 = "V = " + String(Voltage) + " Volt";
      baris2 = "I = " + String(Current) + " Amp";
    }

    displayLCD(baris1, baris2);
    tampilDaya = !tampilDaya;
    waktuDisplaySebelumnya = waktuSekarang;
  }
  // write to bucket BucketId the TempHum resource
}

void displayLCD(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}