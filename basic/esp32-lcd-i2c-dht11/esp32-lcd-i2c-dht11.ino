#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Konfigurasi DHT11
#define DHTPIN 13          // Pin DATA DHT11 di GPIO 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Konfigurasi LCD (Alamat I2C biasanya 0x27 atau 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi DHT
  dht.begin();
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  
  // Tampilan awal
  lcd.setCursor(0, 0);
  lcd.print("Sistem Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Suhu & Kelembaban");
  delay(2000);
  lcd.clear();
}

void loop() {
  // Membaca data dari sensor
  float kelembaban = dht.readHumidity();
  float suhu = dht.readTemperature();

  // Cek jika sensor gagal terbaca
  if (isnan(kelembaban) || isnan(suhu)) {
    Serial.println("Gagal membaca sensor DHT!");
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error!   ");
    return;
  }

  // Tampilkan di Serial Monitor (Opsional)
  Serial.print("Suhu: "); Serial.print(suhu);
  Serial.print("C | Lembab: "); Serial.print(kelembaban);
  Serial.println("%");

  // Tampilkan di LCD
  lcd.setCursor(0, 0);
  lcd.print("Suhu: ");
  lcd.print(suhu);
  lcd.print((char)223); // Simbol derajat
  lcd.print("C   ");

  lcd.setCursor(0, 1);
  lcd.print("Lembab: ");
  lcd.print(kelembaban);
  lcd.print("%   ");

  delay(2000); // Sensor DHT11 butuh jeda minimal 2 detik
}