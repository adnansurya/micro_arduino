#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "GravityTDS.h"

// --- KONFIGURASI LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- KONFIGURASI TDS ---
#define TdsSensorPin A0
GravityTDS gravityTds;
float temperature = 25.0; 
float tdsValue = 0;

// --- KONFIGURASI PIN LAIN ---
const int pinPH     = A1;
const int ledHijau  = 11;
const int ledKuning = 10;
const int ledMerah  = 9;

// --- PARAMETER AMBANG BATAS ---
const float batasTdsNormal = 500.0; 
const float phMinNormal    = 7.0;
const float phMaxNormal    = 10.5;

void setup() {
  Serial.begin(115200);

  pinMode(ledHijau, OUTPUT);
  pinMode(ledKuning, OUTPUT);
  pinMode(ledMerah, OUTPUT);

  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);      
  gravityTds.setAdcRange(1024); 
  gravityTds.begin();

  lcd.init();
  lcd.backlight();

  lcd.setCursor(2, 0);
  lcd.print("AHAS Digital");
  lcd.setCursor(0, 1);
  lcd.print("Coolant Analyzer");
  delay(3000);
  lcd.clear();
}

void loop() {
  // 1. PROSES PEMBACAAN TDS
  gravityTds.setTemperature(temperature); 
  gravityTds.update();
  tdsValue = gravityTds.getTdsValue(); 

  // 2. PROSES PEMBACAAN PH (Dengan Persamaan Linear & Averaging)
  long totalRawPH = 0;
  for(int i=0; i<10; i++) { // Ambil 10 sampel untuk stabilitas
    totalRawPH += analogRead(pinPH);
    delay(10);
  }
  float rataRawPH = totalRawPH / 10.0;
  
  // Rumus hasil kalibrasi: pH = (m * ADC) + c
  float nilaiPH = (-0.0364 * rataRawPH) + 30.36;

  // Batasi nilai agar tetap di rentang 0-14
  if (nilaiPH > 14.0) nilaiPH = 14.0;
  if (nilaiPH < 0.0)  nilaiPH = 0.0;

  // 3. LOGIKA EVALUASI
  bool tdsNormal = (tdsValue <= batasTdsNormal);
  bool phNormal  = (nilaiPH >= phMinNormal && nilaiPH <= phMaxNormal);

  // 4. TAMPILAN LCD
  lcd.setCursor(0, 0);
  lcd.print("TDS:");
  lcd.print((int)tdsValue);
  lcd.print("  "); // Clear sisa angka

  lcd.setCursor(9, 0);
  lcd.print("pH:");
  lcd.print(nilaiPH, 1);
  lcd.print(" ");

  lcd.setCursor(0, 1);
  if (tdsNormal && phNormal) {
    lcd.print("STATUS: NORMAL  ");
    digitalWrite(ledHijau, HIGH);
    digitalWrite(ledKuning, LOW);
    digitalWrite(ledMerah, LOW);
  } 
  else if (!tdsNormal && !phNormal) {
    lcd.print("CHANGE COOLANT!!!");
    digitalWrite(ledHijau, LOW);
    digitalWrite(ledKuning, LOW);
    digitalWrite(ledMerah, HIGH);
  } 
  else {
    lcd.print("CHANGE COOLANT! "); // Salah satu bermasalah
    digitalWrite(ledHijau, LOW);
    digitalWrite(ledKuning, HIGH);
    digitalWrite(ledMerah, LOW);
  }

  delay(1000);
}