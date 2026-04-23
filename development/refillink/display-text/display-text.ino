#include <SPI.h>
#include <TFT_eSPI.h>
#include "qrcode.h"    // Library QR Code

TFT_eSPI tft = TFT_eSPI();
QRCode qrcode;

struct Button {
  int x;
  int y;
  int w;
  int h;
  char label[10];
  uint16_t color;
};

// Penyesuaian koordinat untuk layar 3.5 inch (480x320)
Button btn1 = { 360, 10, 110, 90, "QR 1", TFT_BLUE };
Button btn2 = { 360, 115, 110, 90, "QR 2", TFT_GREEN };
Button btn3 = { 360, 220, 110, 90, "Reset", TFT_RED };

void setup() {
  Serial.begin(115200);

  // Backlight Pin 27 untuk layar 3.5"
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  tft.init();
  tft.setRotation(1);

  // Data kalibrasi yang Anda dapatkan tadi
  uint16_t calData[5] = { 342, 3458, 412, 3117, 7 };
  tft.setTouch(calData);

  drawUI();
}

void loop() {
  uint16_t x = 0, y = 0;

  if (tft.getTouch(&x, &y)) {
    // Tombol 1
    if (x > btn1.x && x < (btn1.x + btn1.w) && y > btn1.y && y < (btn1.y + btn1.h)) {
      updateStatus("Menampilkan: GOOGLE");
      generateQRCode("https://google.com");
      delay(300);
    }

    // Tombol 2
    if (x > btn2.x && x < (btn2.x + btn2.w) && y > btn2.y && y < (btn2.y + btn2.h)) {
      updateStatus("Menampilkan: GITHUB");
      generateQRCode("https://github.com");
      delay(300);
    }

    // Tombol 3 (Reset)
    if (x > btn3.x && x < (btn3.x + btn3.w) && y > btn3.y && y < (btn3.y + btn3.h)) {
      tft.fillRect(0, 0, 350, 320, TFT_WHITE);  // Bersihkan area kiri
      drawHeader();                             // Gambar ulang judul
      updateStatus("Sistem Siap...");
      delay(300);
    }
  }
}

void drawUI() {
  tft.fillScreen(TFT_WHITE);

  drawHeader();
  updateStatus("Silakan Pilih Menu");

  // Gambar Tombol
  drawSingleButton(btn1);
  drawSingleButton(btn2);
  drawSingleButton(btn3);
}

// Fungsi untuk membuat Judul di atas
void drawHeader() {
  tft.fillRect(0, 0, 350, 45, TFT_BLUE); // Gunakan warna cerah agar terlihat
  tft.setCursor(20, 15);                // Gunakan koordinat manual (X, Y)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);                   // Ukuran standar
  tft.print("ESP32 QR GENERATOR");      // Gunakan print sederhana
}

void updateStatus(const char* msg) {
  tft.fillRect(0, 275, 350, 45, TFT_YELLOW); // Background kuning untuk status
  tft.setCursor(20, 290);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.print(msg);
}

void drawSingleButton(Button b) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, b.color);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(b.label, b.x + (b.w / 2), b.y + (b.h / 2));
}

void generateQRCode(const char* msg) {
  // Bersihkan area tengah (antara header dan status)
  tft.fillRect(0, 40, 350, 240, TFT_WHITE);

  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, msg);

  int scale = 6;  // Skala diperbesar untuk layar 3.5" agar QR terlihat besar
  int xOffset = (350 - (qrcode.size * scale)) / 2;
  int yOffset = (320 - (qrcode.size * scale)) / 2;

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(x * scale + xOffset, y * scale + yOffset, scale, scale, TFT_BLACK);
      }
    }
  }
}