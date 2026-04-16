#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "GravityTDS.h"

// --- KONFIGURASI LCD ---
// Alamat I2C umum 0x27. Pin SDA di 2, SCL di 3 (Pro Micro)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- KONFIGURASI TDS ---
#define TdsSensorPin A0
GravityTDS gravityTds;
float temperature = 25.0; 
float tdsValue = 0;

// --- KONFIGURASI PIN LAIN ---
const int pinPH     = A1;
const int ledHijau  = 5;
const int ledKuning = 6;
const int ledMerah  = 7;

// --- PARAMETER AMBANG BATAS (Silakan sesuaikan) ---
const float batasTdsNormal = 500.0; // Contoh: Di atas 500 PPM dianggap problem
const float phMinNormal    = 7.0;
const float phMaxNormal    = 9.0;

void setup() {
  Serial.begin(115200);

  // Inisialisasi LED
  pinMode(ledHijau, OUTPUT);
  pinMode(ledKuning, OUTPUT);
  pinMode(ledMerah, OUTPUT);

  // Inisialisasi TDS (Library Gravity)
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(5.0);      
  gravityTds.setAdcRange(1024); 
  gravityTds.begin();

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();

  // Splash Screen
  lcd.setCursor(2, 0);
  lcd.print("AHAS Digital");
  lcd.setCursor(0, 1);
  lcd.print("Coolant Analyzer");
  delay(3000);
  lcd.clear();
}

void loop() {
  lcd.clear();
  // 1. PROSES PEMBACAAN TDS (PPM)
  gravityTds.setTemperature(temperature); 
  gravityTds.update();
  tdsValue = gravityTds.getTdsValue(); 

  // 2. PROSES PEMBACAAN PH
  int nilaiRawPH = analogRead(pinPH);
  float nilaiPH = nilaiRawPH * (14.0 / 1023.0);

  // 3. LOGIKA EVALUASI
  bool tdsNormal = (tdsValue <= batasTdsNormal);
  bool phNormal  = (nilaiPH >= phMinNormal && nilaiPH <= phMaxNormal);

  // 4. TAMPILAN LCD
  // Baris 1: Menampilkan Angka
  lcd.setCursor(0, 0);
  lcd.print("TDS:");
  lcd.print((int)tdsValue);
  // lcd.print("ppm "); // Menampilkan satuan ppm
  
  lcd.setCursor(9, 0);
  lcd.print("pH:");
  lcd.print(nilaiPH, 1);

  // Baris 2: Menampilkan Status
  lcd.setCursor(0, 1);

  if (tdsNormal && phNormal) {
    // SEMUA NORMAL
    lcd.print("STATUS: NORMAL  ");
    digitalWrite(ledHijau, HIGH);
    digitalWrite(ledKuning, LOW);
    digitalWrite(ledMerah, LOW);
  } 
  else if (!tdsNormal && !phNormal) {
    // DUA-DUANYA PROBLEM
    lcd.print("CHANGE COOLANT! ");
    digitalWrite(ledHijau, LOW);
    digitalWrite(ledKuning, LOW);
    digitalWrite(ledMerah, HIGH);
  } 
  else {
    // SALAH SATU PROBLEM
    lcd.print("CHANGE COOLANT! ");
    digitalWrite(ledHijau, LOW);
    digitalWrite(ledKuning, HIGH);
    digitalWrite(ledMerah, LOW);
  }

  delay(1000);
}