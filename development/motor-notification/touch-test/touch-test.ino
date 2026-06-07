#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

// Pin definitions untuk CYD (standar)
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Inisialisasi SPI untuk touchscreen
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Nyalakan backlight (pin 21 untuk CYD 2.8")
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  
  // === ORIENTASI DISPLAY ===
  // Rotation untuk TFT_eSPI:
  // 0 = portrait (USB bawah)
  // 1 = landscape (USB kanan)
  // 2 = portrait terbalik (USB atas)
  // 3 = landscape terbalik (USB kiri)
  tft.init();
  tft.setRotation(0);  // 3 = landscape terbalik (90° counter clockwise)
  tft.fillScreen(TFT_BLACK);
  
  // === ORIENTASI TOUCHSCREEN ===
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);  // Sama dengan display rotation
  
  // Tampilkan instruksi di layar
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Touchscreen Test", tft.width() / 2, 30, 2);
  tft.drawCentreString("Sentuh layar di mana saja", tft.width() / 2, 60, 1);
  tft.drawCentreString("Lihat Serial Monitor", tft.width() / 2, 90, 1);
  
  Serial.println("=== CYD TOUCHSCREEN TEST ===");
  Serial.println("Rotation Display: 3 (90° counter clockwise)");
  Serial.println("Rotation Touch: 3");
  Serial.println("Sentuh layar untuk melihat koordinat");
}

void loop() {
  // Cek apakah layar disentuh
  if (touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    
    // Koordinat raw dari touchscreen (dalam rotation 3)
    int rawX = p.x;
    int rawY = p.y;
    
    // Kalibrasi koordinat ke resolusi layar (320x240)
    // Sesuaikan nilai map ini setelah kalibrasi
    int x = map(rawX, 200, 3700, 0, 240);
    int y = map(rawY, 240, 3800, 0, 320);
    
    // Tampilkan di Serial Monitor
    Serial.print("Raw X=");
    Serial.print(rawX);
    Serial.print(" Raw Y=");
    Serial.print(rawY);
    Serial.print(" | Cal X=");
    Serial.print(x);
    Serial.print(" Y=");
    Serial.println(y);
    
    // Tampilkan di layar
    tft.fillRect(10, 120, tft.width() - 20, 40, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("X=" + String(x) + " Y=" + String(y), 20, 130, 2);
    
    // Gambar titik kecil di posisi sentuhan
    if (x > 0 && x < 320 && y > 0 && y < 240) {
      tft.fillCircle(x, y, 3, TFT_YELLOW);
      delay(100);
      tft.fillCircle(x, y, 3, TFT_BLACK);
    }
    
    delay(150);  // Debounce
  }
  
  delay(50);
}