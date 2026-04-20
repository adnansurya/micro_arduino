#include <SPI.h>
#include <TFT_eSPI.h>  // Library Grafis
#include "qrcode.h"    // Library QR Code

TFT_eSPI tft = TFT_eSPI();
QRCode qrcode;

// Definisi area tombol (X, Y, Lebar, Tinggi)
struct Button {
  int x;
  int y;
  int w;
  int h;
  char label[10];
  uint16_t color;
};
// Sesuaikan posisi tombol untuk layar 480x320
Button btn1 = { 350, 20, 110, 80, "QR 1", TFT_BLUE };
Button btn2 = { 350, 120, 110, 80, "QR 2", TFT_GREEN };
Button btn3 = { 350, 220, 110, 80, "Reset", TFT_RED };

void setup() {
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);  // Menyalakan lampu latar
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);  // Mode Landscape

  // Kalibrasi layar sentuh (Sesuaikan angka ini jika posisi sentuh meleset)
  uint16_t calData[5] = { 342, 3458, 412, 3117, 7 };
  tft.setTouch(calData);

  drawUI();
}

// void setup() {
//   Serial.begin(115200);
  
//   // Power on Backlight (Pin 27 untuk 3.5")
//   pinMode(27, OUTPUT);
//   digitalWrite(27, HIGH);

//   tft.init();
//   tft.setRotation(1); 

//   // --- PROSES KALIBRASI MANUAL ---
//   uint16_t calData[5];
//   bool repeat = false;
  
//   // Perintah kalibrasi: 
//   // Jika gagal terdeteksi, biasanya program akan stuck di sini
//   tft.calibrateTouch(calData, TFT_WHITE, TFT_RED, 15);
  
//   Serial.println("Kalibrasi selesai. Masukkan angka ini ke calData:");
//   for (uint8_t i = 0; i < 5; i++) Serial.println(calData[i]);

//   tft.setTouch(calData);
//   drawUI();
// }

void loop() {
  uint16_t x = 0, y = 0;

  // Cek jika layar ditekan
  if (tft.getTouch(&x, &y)) {
    Serial.print(x);
    Serial.print(",");
    Serial.println(y);

    // Cek Tombol 1
    if (x > btn1.x && x < (btn1.x + btn1.w) && y > btn1.y && y < (btn1.y + btn1.h)) {
      generateQRCode("https://google.com");
      delay(300);
    }

    // Cek Tombol 2
    if (x > btn2.x && x < (btn2.x + btn2.w) && y > btn2.y && y < (btn2.y + btn2.h)) {
      generateQRCode("https://github.com");
      delay(300);
    }

    // Cek Tombol 3 (Reset)
    if (x > btn3.x && x < (btn3.x + btn3.w) && y > btn3.y && y < (btn3.y + btn3.h)) {
      tft.fillRect(0, 0, 210, 240, TFT_WHITE);  // Bersihkan area QR saja
      delay(300);
    }
  }
}

// Fungsi menggambar tampilan utama
void drawUI() {
  tft.fillScreen(TFT_WHITE);

  // Gambar Tombol-tombol
  drawSingleButton(btn1);
  drawSingleButton(btn2);
  drawSingleButton(btn3);
}

void drawSingleButton(Button b) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, b.color);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(b.label, b.x + (b.w / 2), b.y + (b.h / 2));
}

// Fungsi untuk Generate dan Gambar QR Code
void generateQRCode(const char* msg) {
  // Bersihkan area kiri sebelum gambar QR baru
  tft.fillRect(0, 0, 340, 320, TFT_WHITE);

  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, msg);

  // Tentukan ukuran blok (scale) agar pas di layar
  int scale = 4;
  int xOffset = (210 - (qrcode.size * scale)) / 2;
  int yOffset = (240 - (qrcode.size * scale)) / 2;

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(x * scale + xOffset, y * scale + yOffset, scale, scale, TFT_BLACK);
      }
    }
  }
}