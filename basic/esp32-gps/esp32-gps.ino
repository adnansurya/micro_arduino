#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPS++.h>

// Konfigurasi Layar OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Konfigurasi GPS Pins (Menggunakan UART2 ESP32)
#define RXD2 16
#define TXD2 17
#define GPS_BAUDRATE 9600

// Inisialisasi Objek TinyGPS++
TinyGPSPlus gps;

void setup() {
  // Serial Monitor untuk Debugging
  Serial.begin(115200);
  
  // Inisialisasi Serial2 untuk GPS
  Serial2.begin(GPS_BAUDRATE, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Mulai membaca data GPS NEO-M8N...");

  // Inisialisasi Layar OLED (Alamat I2C umumnya 0x3C)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("OLED gagal diinisialisasi, periksa kabel!"));
    for(;;);
  }
  
  // Tampilan awal (Splash Screen)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println("GPS NEO-M8N Ready");
  display.setCursor(10, 40);
  display.println("Mencari Satelit...");
  display.display();
  delay(2000);
}

void loop() {
  // Membaca data dari modul GPS
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      tampilkanDataGPS();
    }
  }

  // Jika dalam 5 detik tidak ada data masuk dari GPS, munculkan peringatan di Serial
  if (millis() > 5000 && gps.charsProcessed() < 10) {
    Serial.println(F("GPS tidak terdeteksi, periksa kabel TX/RX!"));
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Error: GPS tidak");
    display.println("terdeteksi.");
    display.display();
    delay(1000);
  }
}

void tampilkanDataGPS() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Jika koordinat GPS sudah valid (sudah mengunci satelit)
  if (gps.location.isValid()) {
    // Baris 1: Latitude
    display.setCursor(0, 0);
    display.print("Lat : ");
    display.print(gps.location.lat(), 6);

    // Baris 2: Longitude
    display.setCursor(0, 12);
    display.print("Lng : ");
    display.print(gps.location.lng(), 6);

    // Baris 3: Kecepatan (Kmph) & Jumlah Satelit
    display.setCursor(0, 24);
    display.print("Spd : ");
    display.print(gps.speed.kmph(), 1);
    display.print(" km/h");

    // Baris 4: Ketinggian (Altitude)
    display.setCursor(0, 36);
    display.print("Alt : ");
    display.print(gps.altitude.meters(), 1);
    display.print(" m");

    // Baris 5: Jumlah Satelit aktif
    display.setCursor(0, 48);
    display.print("Sat : ");
    display.print(gps.satellites.value());
    
    // Cetak juga ke Serial Monitor untuk backup
    Serial.print("LAT: "); Serial.print(gps.location.lat(), 6);
    Serial.print(" | LNG: "); Serial.println(gps.location.lng(), 6);
  } 
  // Jika GPS terhubung tapi belum mendapatkan kunci satelit yang cukup
  else {
    display.setCursor(0, 0);
    display.println("Mencari Satelit...");
    display.setCursor(0, 20);
    display.print("Satelit Terkoneksi: ");
    display.print(gps.satellites.value());
    
    Serial.print("Mencari Satelit... Terkoneksi: ");
    Serial.println(gps.satellites.value());
  }

  display.display();
}