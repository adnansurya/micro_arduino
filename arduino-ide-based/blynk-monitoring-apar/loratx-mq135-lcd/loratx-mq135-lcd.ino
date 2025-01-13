#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Konfigurasi pin LoRa
#define SS 5
#define RST 14
#define DIO0 26

// Konfigurasi pin MQ135
const int analogPinMQ135 = 34; // Pin analog untuk MQ135
const float RL = 10.0;    // Resistor load dalam kilo-ohm
const float VCC = 3.3;    // Tegangan catu daya ESP32
const float ratioCleanAir = 3.62; // Rasio RS/R0 di udara bersih

float R0 = 1.0; // Nilai referensi R0 (lakukan kalibrasi sebelumnya)

// Konfigurasi sensor tegangan
const int analogPinVoltage = 35; // Pin analog untuk sensor tegangan
const float voltageDividerRatio = 5.0; // Rasio pembagi tegangan (sesuaikan dengan sensor Anda)

// Konfigurasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C LCD biasanya 0x27

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Init...");

  // Inisialisasi LoRa
  Serial.println("Menginisialisasi LoRa...");
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(915E6)) { // Frekuensi default 915 MHz
    Serial.println("Gagal menginisialisasi LoRa!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LoRa Error!");
    while (1);
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
  lcd.print(R0, 2); // Tampilkan nilai R0 dengan 2 desimal
  delay(2000);
}

void loop() {
  // Membaca data PPM dari MQ135
  float rs = readSensorMQ135();
  float ratio = rs / R0;
  float ppm = pow(10, ((log10(ratio) - (-0.45)) / (-0.32))); // m dan b dari datasheet

  // Membaca persentase ADC dari MQ135
  int adcValueMQ135 = analogRead(analogPinMQ135);
  float adcPercentageMQ135 = (adcValueMQ135 / 4095.0) * 100.0;

  // Membaca tegangan dari sensor tegangan
  float voltage = readVoltage();

  // Kirim data melalui LoRa
  Serial.print("Mengirim PPM CO: ");
  Serial.print(ppm);
  Serial.print(" ppm, Tegangan: ");
  Serial.print(voltage);
  Serial.println(" V");

  LoRa.beginPacket();
  LoRa.print("PPM: ");
  LoRa.print(ppm);
  LoRa.print(", Tegangan: ");
  LoRa.print(voltage);
  LoRa.print(" V");
  LoRa.endPacket();

  // Tampilkan data pada LCD
  displayOnLCD(ppm, adcPercentageMQ135, voltage);

  delay(2000); // Delay 2 detik sebelum pengukuran berikutnya
}

float readSensorMQ135() {
  int adcValue = analogRead(analogPinMQ135);
  float sensorVoltage = (adcValue / 4095.0) * VCC; // Konversi ke tegangan (12-bit ADC)
  float rs = ((VCC - sensorVoltage) / sensorVoltage) * RL; // Hitung RS
  return rs;
}

float calibrateSensor() {
  float totalRS = 0;
  for (int i = 0; i < 100; i++) { // Ambil rata-rata dari 100 pembacaan
    totalRS += readSensorMQ135();
    delay(50);
  }
  float avgRS = totalRS / 100.0;
  return avgRS / ratioCleanAir; // Hitung R0 berdasarkan rasio udara bersih
}

float readVoltage() {
  int adcValue = analogRead(analogPinVoltage);        // Baca nilai analog
  float sensorVoltage = (adcValue / 4095.0) * VCC;    // Tegangan pada pin ADC
  return sensorVoltage * voltageDividerRatio;         // Hitung tegangan aktual
}

void displayOnLCD(float ppm, float adcPercentage, float voltage) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PPM: ");
  lcd.print(ppm, 2); // Tampilkan nilai PPM dengan 2 desimal

  lcd.setCursor(0, 1);
  lcd.print("V: ");
  lcd.print(voltage, 2); // Tampilkan tegangan dengan 2 desimal
  lcd.print(" ADC: ");
  lcd.print(adcPercentage, 1); // Tampilkan persentase ADC dengan 1 desimal
}
