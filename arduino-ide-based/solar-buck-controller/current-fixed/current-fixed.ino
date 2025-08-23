#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 20, 4); // Ganti alamat I2C sesuai modul Anda

// Pin untuk sensor
const int currentSensor1 = A0;
const int currentSensor2 = A1;
const int voltageSensor1 = A2;
const int voltageSensor2 = A3;

// Konstanta untuk perhitungan
const float VREF = 5.0;           // Tegangan referensi Arduino (5V)
const int ADC_RESOLUTION = 1023;  // Resolusi ADC 10-bit

// Kalibrasi sensor ACS712
const float SENSITIVITY = 0.185;  // 185mV/A untuk ACS712 5A
const float ZERO_CURRENT_OFFSET = 2.5; // 2.5V offset saat tidak ada arus

// Kalibrasi sensor tegangan
const float R1 = 30000.0; // Resistor 30k ohm
const float R2 = 7500.0;  // Resistor 7.5k ohm
const float VOLTAGE_FACTOR = VREF * (R1 + R2) / (R2 * ADC_RESOLUTION);

// Variabel untuk kalibrasi dan filter
float offset1 = 0;
float offset2 = 0;
const int CALIBRATION_SAMPLES = 500;
const int NUM_SAMPLES = 10; // Filter moving average

void setup() {
  Serial.begin(9600);
  
  // Inisialisasi LCD I2C
  lcd.init();
  lcd.backlight();
  
  // Tampilkan pesan awal
  lcd.setCursor(0, 0);
  lcd.print("Calibrating Sensors");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  
  // Kalibrasi sensor arus (pastikan tidak ada arus mengalir)
  calibrateCurrentSensors();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibration Complete");
  delay(1000);
  lcd.clear();
}

void calibrateCurrentSensors() {
  long sum1 = 0;
  long sum2 = 0;
  
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sum1 += analogRead(currentSensor1);
    sum2 += analogRead(currentSensor2);
    delay(5);
  }
  
  offset1 = sum1 / CALIBRATION_SAMPLES;
  offset2 = sum2 / CALIBRATION_SAMPLES;
  
  Serial.print("Offset Sensor 1: ");
  Serial.println(offset1);
  Serial.print("Offset Sensor 2: ");
  Serial.println(offset2);
}

float readVoltageDC(int sensorPin) {
  int adcValue = analogRead(sensorPin);
  
  // Pastikan nilai ADC valid
  if (adcValue < 0 || adcValue > ADC_RESOLUTION) {
    return 0.0;
  }
  
  float voltage = adcValue * VOLTAGE_FACTOR;
  return voltage;
}

float readCurrentDC(int sensorPin, float offset) {
  int adcValue = analogRead(sensorPin);
  
  // Pastikan nilai ADC valid
  if (adcValue < 0 || adcValue > ADC_RESOLUTION) {
    return 0.0;
  }
  
  float voltage = (adcValue * VREF) / ADC_RESOLUTION;
  
  // Hindari pembagian dengan nilai yang sangat kecil
  if (abs(voltage - ZERO_CURRENT_OFFSET) < 0.001) {
    return 0.0;
  }
  
  float current = (voltage - ZERO_CURRENT_OFFSET) / SENSITIVITY;
  
  // Kompensasi offset
  current = current - ((offset * VREF / ADC_RESOLUTION) - ZERO_CURRENT_OFFSET) / SENSITIVITY;
  
  // Filter nilai yang tidak masuk akal
  if (abs(current) > 100) { // Batasi nilai arus maksimum
    return 0.0;
  }
  
  return current;
}

void loop() {
  // Baca tegangan DC dengan filter
  static float voltage1Samples[NUM_SAMPLES];
  static float voltage2Samples[NUM_SAMPLES];
  static float current1Samples[NUM_SAMPLES];
  static float current2Samples[NUM_SAMPLES];
  static int sampleIndex = 0;
  
  // Baca nilai baru
  voltage1Samples[sampleIndex] = readVoltageDC(voltageSensor1);
  voltage2Samples[sampleIndex] = readVoltageDC(voltageSensor2);
  current1Samples[sampleIndex] = readCurrentDC(currentSensor1, offset1);
  current2Samples[sampleIndex] = readCurrentDC(currentSensor2, offset2);
  
  // Hitung rata-rata
  float voltage1 = 0, voltage2 = 0, current1 = 0, current2 = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    voltage1 += voltage1Samples[i];
    voltage2 += voltage2Samples[i];
    current1 += current1Samples[i];
    current2 += current2Samples[i];
  }
  
  voltage1 /= NUM_SAMPLES;
  voltage2 /= NUM_SAMPLES;
  current1 /= NUM_SAMPLES;
  current2 /= NUM_SAMPLES;
  
  sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;
  
  // Tampilkan hasil di LCD
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
  if (current1 >= -0.001 && current1 < 0.001) {
    lcd.print(" 0.000");
  } else {
    if (current1 >= 0) lcd.print(" ");
    lcd.print(current1, 3);
  }
  lcd.print("A  ");
  
  lcd.setCursor(0, 3);
  lcd.print("I2:");
  if (current2 >= -0.001 && current2 < 0.001) {
    lcd.print(" 0.000");
  } else {
    if (current2 >= 0) lcd.print(" ");
    lcd.print(current2, 3);
  }
  lcd.print("A  ");
  
  // // Tampilkan status
  // lcd.setCursor(0, 2);
  // lcd.print("DC Measurement Active");
  
  // lcd.setCursor(0, 3);
  // lcd.print("Stable Reading Filter");
  
  // Tampilkan di Serial Monitor untuk debugging
  Serial.print("V1: ");
  Serial.print(voltage1, 2);
  Serial.print("V, V2: ");
  Serial.print(voltage2, 2);
  Serial.print("V, I1: ");
  Serial.print(current1, 3);
  Serial.print("A, I2: ");
  Serial.print(current2, 3);
  Serial.println("A");
  
  delay(500);
}