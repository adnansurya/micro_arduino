#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "X9C10X.h"

// Pin untuk Potensiometer 1
#define POT1_INC_PIN 12
#define POT1_UD_PIN 11
#define POT1_CS_PIN 10

// Pin untuk Potensiometer 2
#define POT2_INC_PIN 8
#define POT2_UD_PIN 7
#define POT2_CS_PIN 6

// Buat 2 instance object potensio
X9C10X pot1(12345);
X9C10X pot2(12345);

// Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Ganti alamat I2C sesuai modul Anda

// Pin untuk sensor
const int currentSensor1 = A0;
const int currentSensor2 = A1;
const int voltageSensor1 = A2;
const int voltageSensor2 = A3;

// Konstanta untuk perhitungan
const float VREF = 5.0;           // Tegangan referensi Arduino (5V)
const int ADC_RESOLUTION = 1023;  // Resolusi ADC 10-bit

// Kalibrasi sensor ACS712
const float SENSITIVITY = 0.185;        // 185mV/A untuk ACS712 5A
const float ZERO_CURRENT_OFFSET = 2.5;  // 2.5V offset saat tidak ada arus

// Kalibrasi sensor tegangan
const float R1 = 30000.0;  // Resistor 30k ohm
const float R2 = 7500.0;   // Resistor 7.5k ohm
const float VOLTAGE_FACTOR = VREF * (R1 + R2) / (R2 * ADC_RESOLUTION);

// Variabel untuk kalibrasi dan filter
float offset1 = 0;
float offset2 = 0;
const int CALIBRATION_SAMPLES = 500;
const int NUM_SAMPLES = 10;  // Filter moving average

// Variabel untuk timing dengan millis()
unsigned long previousMillis = 0;
const long DISPLAY_INTERVAL = 500;  // Interval update tampilan (ms)
unsigned long calibrationStartTime = 0;
const long CALIBRATION_DURATION = 2500;  // Durasi kalibrasi (ms)

// Status variables
bool calibrationComplete = false;
int calibrationStep = 0;

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

  // Inisialisasi Potensiometer 1 - posisi tengah
  pot1.begin(POT1_INC_PIN, POT1_UD_PIN, POT1_CS_PIN);  
  pot1.setPosition(50);  // Posisi tengah

  // Inisialisasi Potensiometer 2 - posisi tengah
  pot2.begin(POT2_INC_PIN, POT2_UD_PIN, POT2_CS_PIN);  
  pot2.setPosition(50);  // Posisi tengah

  calibrationStartTime = millis();
}

void calibrateCurrentSensors() {
  long sum1 = 0;
  long sum2 = 0;

  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sum1 += analogRead(currentSensor1);
    sum2 += analogRead(currentSensor2);

    // Non-blocking delay untuk kalibrasi
    if (i % 50 == 0) {
      updateCalibrationProgress(i * 100 / CALIBRATION_SAMPLES);
    }
  }

  offset1 = sum1 / CALIBRATION_SAMPLES;
  offset2 = sum2 / CALIBRATION_SAMPLES;

  Serial.print("Offset Sensor 1: ");
  Serial.println(offset1);
  Serial.print("Offset Sensor 2: ");
  Serial.println(offset2);
}

void updateCalibrationProgress(int progress) {
  lcd.setCursor(0, 2);
  lcd.print("Progress: ");
  lcd.print(progress);
  lcd.print("%   ");
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
  if (abs(current) > 100) {  // Batasi nilai arus maksimum
    return 0.0;
  }

  return current;
}

// Fungsi untuk memproses perintah serial
void processSerialCommand(String input) {
  input.trim(); // Hilangkan spasi di awal dan akhir
  
  if (input.length() > 0) {
    Serial.print("Received: ");
    Serial.println(input);
    
    // Periksa perintah untuk Potensiometer 1 (format: 1,XX)
    if (input.startsWith("1,")) {
      String valueStr = input.substring(2);
      int value = valueStr.toInt();
      
      if (value >= 0 && value <= 99) {
        pot1.setPosition(value);
        Serial.print("Pot1 diatur ke posisi: ");
        Serial.println(value);
      } else {
        Serial.println("Error: Nilai harus antara 0-99");
      }
    }
    // Periksa perintah untuk Potensiometer 2 (format: 2,XX)
    else if (input.startsWith("2,")) {
      String valueStr = input.substring(2);
      int value = valueStr.toInt();
      
      if (value >= 0 && value <= 99) {
        pot2.setPosition(value);
        Serial.print("Pot2 diatur ke posisi: ");
        Serial.println(value);
      } else {
        Serial.println("Error: Nilai harus antara 0-99");
      }
    }
    else {
      Serial.println("Error: Perintah tidak dikenali");
      Serial.println("Gunakan format: 1,XX atau 2,XX");
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Baca data dari serial port menggunakan readStringUntil()
  if (Serial.available() > 0) {
    String serialInput = Serial.readStringUntil('\n');
    processSerialCommand(serialInput);
  }

  // Proses kalibrasi
  if (!calibrationComplete) {
    if (calibrationStep == 0) {
      // Tunggu sebentar sebelum mulai kalibrasi
      if (currentMillis - calibrationStartTime >= 1000) {
        calibrationStep = 1;
        calibrationStartTime = currentMillis;
      }
    } else if (calibrationStep == 1) {
      // Lakukan kalibrasi
      calibrateCurrentSensors();
      calibrationStep = 2;
      calibrationStartTime = currentMillis;
    } else if (calibrationStep == 2) {
      // Tampilkan pesan kalibrasi selesai
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Calibration Complete");

      if (currentMillis - calibrationStartTime >= 1000) {
        calibrationComplete = true;
        lcd.clear();
      }
    }
    return;
  }

  // Pembacaan sensor dan update tampilan dengan interval tertentu
  if (currentMillis - previousMillis >= DISPLAY_INTERVAL) {
    previousMillis = currentMillis;

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
  }

  // Di sini bisa ditambahkan kode lain yang perlu dijalankan tanpa blocking
}