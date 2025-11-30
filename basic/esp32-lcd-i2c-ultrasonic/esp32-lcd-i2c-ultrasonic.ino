#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Definisi pin untuk sensor HC-SR04
const int trigPin = 5;
const int echoPin = 18;

// Inisialisasi LCD I2C (alamat I2C biasanya 0x27 atau 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C 0x27, LCD 16x2

// Variabel untuk pengukuran jarak
long duration;
float distance;
float distanceCm;

void setup() {
  // Inisialisasi Serial Monitor
  Serial.begin(115200);
  
  // Inisialisasi pin sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Tampilkan pesan awal
  lcd.setCursor(0, 0);
  lcd.print("Sensor HC-SR04");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  delay(2000);
  lcd.clear();
}

void loop() {
  // Bersihkan trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Set trigger pin HIGH selama 10 microsecond
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Baca echo pin, mengembalikan waktu tempuh suara dalam microsecond
  duration = pulseIn(echoPin, HIGH);
  
  // Hitung jarak
  distanceCm = duration * 0.034 / 2;
  
  // Tampilkan di Serial Monitor
  Serial.print("Jarak: ");
  Serial.print(distanceCm);
  Serial.println(" cm");
  
  // Tampilkan di LCD
  lcd.setCursor(0, 0);
  lcd.print("Jarak: ");
  lcd.print(distanceCm);
  lcd.print(" cm  "); // Spasi tambahan untuk menghapus karakter sebelumnya
  
  // Tampilkan status
  lcd.setCursor(0, 1);
  if (distanceCm < 5) {
    lcd.print("Terlalu Dekat! ");
  } else if (distanceCm < 20) {
    lcd.print("Dekat         ");
  } else if (distanceCm < 100) {
    lcd.print("Jarak Normal  ");
  } else if (distanceCm <= 400) {
    lcd.print("Jarak Jauh    ");
  } else {
    lcd.print("Di Luar Jangkauan");
  }
  
  delay(500); // Delay antara pengukuran
}