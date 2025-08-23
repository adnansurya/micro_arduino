#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ACS712.h>

// Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Inisialisasi sensor ACS712
ACS712 acs1(A0, ACS712_05B);
ACS712 acs2(A1, ACS712_05B);

// Pin untuk sensor tegangan DC
const int voltageSensor1 = A2;
const int voltageSensor2 = A3;

// Konstanta untuk perhitungan tegangan
// const float VREF = 5.0;
const int ADC_RESOLUTION = 1023;
const float R1 = 30000.0;
const float R2 = 7500.0;
const float VOLTAGE_FACTOR = VREF * (R1 + R2) / (R2 * ADC_RESOLUTION);

// Filter untuk pembacaan yang stabil
const int NUM_SAMPLES = 50;
float voltage1Samples[NUM_SAMPLES];
float voltage2Samples[NUM_SAMPLES];
float current1Samples[NUM_SAMPLES];
float current2Samples[NUM_SAMPLES];
int sampleIndex = 0;

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // Inisialisasi array samples
  for (int i = 0; i < NUM_SAMPLES; i++) {
    voltage1Samples[i] = 0;
    voltage2Samples[i] = 0;
    current1Samples[i] = 0;
    current2Samples[i] = 0;
  }

  // Kalibrasi sensor ACS712
  acs1.calibrate();
  acs2.calibrate();

  lcd.setCursor(0, 0);
  lcd.print("DC Measurement System");
  lcd.setCursor(0, 3);
  lcd.print("Calibrating...");
  delay(2000);
  lcd.clear();
}

float readFilteredVoltage(int sensorPin, float samples[]) {
  // Baca nilai baru
  int adcValue = analogRead(sensorPin);
  float newVoltage = adcValue * VOLTAGE_FACTOR;

  // Update array samples
  samples[sampleIndex] = newVoltage;

  // Hitung rata-rata
  float sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += samples[i];
  }

  return sum / NUM_SAMPLES;
}

float readFilteredCurrent(ACS712 &sensor, float samples[]) {
  // Baca nilai baru
  float newCurrent = sensor.getCurrentDC();

  // Update array samples
  samples[sampleIndex] = newCurrent;

  // Hitung rata-rata
  float sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += samples[i];
  }

  return sum / NUM_SAMPLES;
}

void loop() {
  // Baca nilai dengan filter
  float voltage1 = readFilteredVoltage(voltageSensor1, voltage1Samples);
  float voltage2 = readFilteredVoltage(voltageSensor2, voltage2Samples);
  float current1 = readFilteredCurrent(acs1, current1Samples);
  float current2 = readFilteredCurrent(acs2, current2Samples);

  // float current1 = acs1.getCurrentDC();
  // float current2 = acs2.getCurrentAC();

  // Update sample index
  sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;

  // Tampilkan hasil di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V1:");
  if (voltage1 < 10) lcd.print(" ");
  lcd.print(voltage1, 2);
  lcd.print("V  ");

  lcd.setCursor(0, 2);
  lcd.print("V2:");
  if (voltage2 < 10) lcd.print(" ");
  lcd.print(voltage2, 2);
  lcd.print("V  ");

  lcd.setCursor(0, 1);
  lcd.print("I1:");
  if (current1 >= 0) lcd.print(" ");
  lcd.print(current1, 3);
  lcd.print("A  ");

  lcd.setCursor(0, 3);
  lcd.print("I2:");
  if (current2 >= 0) lcd.print(" ");
  lcd.print(current2, 3);
  lcd.print("A  ");

  // Tampilkan informasi tambahan
  // lcd.setCursor(0, 2);
  // lcd.print("DC Measurement Active");

  // lcd.setCursor(0, 3);
  // lcd.print("Stable Reading Filter");

  // Tampilkan di Serial Monitor
  Serial.print("V1: ");
  Serial.print(voltage1, 2);
  Serial.print("V, V2: ");
  Serial.print(voltage2, 2);
  Serial.print("V, I1: ");
  Serial.print(current1, 3);
  Serial.print("A, I2: ");
  Serial.print(current2, 3);
  Serial.println("A");

  delay(500);  // Update setiap 0.5 detik
}