#include <SPI.h>
#include <TFT_eSPI.h>
#include "qrcode.h"

TFT_eSPI tft = TFT_eSPI();
QRCode qrcode;

enum Page { MENU, QR_PAGE };
Page currentPage = MENU;

// Warna Estetik (Hex 565)
#define COLOR_BG      0xFFFF // Putih Bersih
#define COLOR_HEADER  0x2104 // Abu-abu sangat gelap (Hampir Hitam)
#define COLOR_SUB     0x8410 // Abu-abu Medium
#define COLOR_BLUE    0x2317 // Modern Blue
#define COLOR_GREEN   0x14A8 // Emerald Green
#define COLOR_ORANGE  0xE400 // Deep Orange
#define COLOR_RED     0xC000 // Soft Red

struct Button {
  int x, y, w, h;
  char label[15];
  uint16_t color;
  int amount;
};

Button btn1 = { 30, 100, 260, 75, "500 ml",  COLOR_BLUE,   2000 };
Button btn2 = { 30, 190, 260, 75, "750 ml",  COLOR_GREEN,  3000 };
Button btn3 = { 30, 280, 260, 75, "1 LITER", COLOR_ORANGE, 4000 };
Button btnBack = { 30, 410, 260, 50, "BATAL", COLOR_RED, 0 };

void setup() {
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH); 
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0); 

  uint16_t calData[5] = { 444, 2897, 423, 3309, 4 }; 
  tft.setTouch(calData);

  drawMenuPage();
}

void loop() {
  uint16_t x = 0, y = 0;
  if (tft.getTouch(&x, &y)) {
    if (currentPage == MENU) {
      if (checkBtn(x, y, btn1)) openQRPage(btn1);
      else if (checkBtn(x, y, btn2)) openQRPage(btn2);
      else if (checkBtn(x, y, btn3)) openQRPage(btn3);
    } 
    else if (currentPage == QR_PAGE) {
      if (checkBtn(x, y, btnBack)) {
        currentPage = MENU;
        drawMenuPage();
      }
    }
    delay(200); 
  }
}

bool checkBtn(int tx, int ty, Button b) {
  return (tx > b.x && tx < (b.x + b.w) && ty > b.y && ty < (b.y + b.h));
}

// --- UI HELPER ---
void drawHeader(const char* title, const char* subtitle) {
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(3);
  tft.setCursor(30, 20);
  tft.print(title);
  
  tft.drawFastHLine(30, 55, 260, COLOR_HEADER); // Garis bawah header

  tft.setTextColor(COLOR_SUB);
  tft.setTextSize(2);
  tft.setCursor(30, 65);
  tft.print(subtitle);
}

void drawMenuPage() {
  currentPage = MENU;
  tft.fillScreen(COLOR_BG);
  drawHeader("Isi Ulang", "Pilih takaran air:");
  
  drawSingleButton(btn1);
  drawSingleButton(btn2);
  drawSingleButton(btn3);
}

void openQRPage(Button b) {
  currentPage = QR_PAGE;
  tft.fillScreen(COLOR_BG);
  drawHeader("Bayar", "Scan QRIS di bawah:");

  // Info Box
  tft.fillRoundRect(30, 100, 260, 60, 8, 0xF7BE); // Background abu muda
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(2);
  tft.setCursor(45, 110);
  tft.print(b.label);
  tft.setCursor(45, 135);
  tft.setTextColor(COLOR_BLUE);
  tft.print("Rp "); tft.print(b.amount);

  generateQRCode("https://midtrans.com/sample-qris"); // Nanti ganti dengan URL Dinamis

  drawSingleButton(btnBack);
}

void drawSingleButton(Button b) {
  // Bayangan tombol (Shadow)
  tft.fillRoundRect(b.x + 3, b.y + 3, b.w, b.h, 10, 0xD6BA); 
  // Tombol utama
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 10, b.color);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  
  // Center Text Logic
  int textWidth = strlen(b.label) * 18;
  tft.setCursor(b.x + (b.w - textWidth) / 2, b.y + (b.h - 24) / 2);
  tft.print(b.label);
}

void generateQRCode(const char* msg) {
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, 0, msg);
  
  int scale = 6; 
  int size = qrcode.size * scale;
  int xOffset = (320 - size) / 2;
  int yOffset = 180; 

  // Frame Putih untuk QR
  tft.fillRect(xOffset - 10, yOffset - 10, size + 20, size + 20, TFT_WHITE);
  tft.drawRect(xOffset - 11, yOffset - 11, size + 22, size + 22, COLOR_SUB);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(x * scale + xOffset, y * scale + yOffset, scale, scale, COLOR_HEADER);
      }
    }
  }
}