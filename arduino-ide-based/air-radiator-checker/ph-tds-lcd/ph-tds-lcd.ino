#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi LCD I2C (Alamat biasanya 0x27 atau 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- PIN DEFINITION ---
const int pinTDS    = A0;  // Sensor TDS
const int pinPH     = A1;  // Sensor pH
const int ledHijau  = 5;   // LED Hijau (Pindah ke pin 5)
const int ledKuning = 6;   // LED Kuning (Pindah ke pin 6)
const int ledMerah  = 7;   // LED Merah (Pindah ke pin 7)

// Pin 2 (SDA) dan Pin 3 (SCL) digunakan otomatis oleh <Wire.h> untuk LCD

// Batas Parameter (Silakan sesuaikan dengan standar coolant Anda)
float batasTdsMax = 500.0; // Contoh batas TDS
float batasPhMin  = 7.0;   // Contoh pH minimal
float batasPhMax  = 9.0;   // Contoh pH maksimal

void setup() {
  pinMode(ledHijau, OUTPUT);
  pinMode(ledKuning, OUTPUT);
  pinMode(ledMerah, OUTPUT);
  
  lcd.init();
  lcd.backlight();

  // Tampilan Awal (Splash Screen)
  lcd.setCursor(2, 0);
  lcd.print("AHAS Digital");
  lcd.setCursor(0, 1);
  lcd.print("Coolant Analyzer");
  delay(3000);
  lcd.clear();
}

void loop() {
  // Pembacaan Sensor (Sederhana - Perlu rumus kalibrasi spesifik sensor Anda)
  float nilaiTDS = analogRead(pinTDS) * (1000.0 / 1023.0); // Dummy conversion
  float nilaiPH = analogRead(pinPH) * (14.0 / 1023.0);    // Dummy conversion

  bool tdsNormal = (nilaiTDS <= batasTdsMax);
  bool phNormal  = (nilaiPH >= batasPhMin && nilaiPH <= batasPhMax);

  // Tampilkan Nilai di LCD
  lcd.setCursor(0, 0);
  lcd.print("TDS:");
  lcd.print((int)nilaiTDS);
  lcd.print(" pH:");
  lcd.print(nilaiPH, 1);

  lcd.setCursor(0, 1);

  // Logika Kondisi
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

  delay(1000); // Update setiap 1 detik
}