#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h> // Library WiFiManager
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "qrcode.h"



TFT_eSPI tft = TFT_eSPI();
QRCode qrcode;

// GANTI dengan URL Web App Deployment Google Apps Script Anda
const char* appScriptUrl = "https://script.google.com/macros/s/AKfycbyk4k2RuVMENqUIE1uo3RJmTUop-oImhrTKh37as9pRDvWvYMg5viPCgTtGobY5XQdS/exec";

enum Page { MENU, QR_PAGE, LOADING };
Page currentPage = MENU;

// Warna Estetik
#define COLOR_BG      0xFFFF 
#define COLOR_HEADER  0x2104 
#define COLOR_SUB     0x8410 
#define COLOR_BLUE    0x2317 
#define COLOR_GREEN   0x14A8 
#define COLOR_ORANGE  0xE400 
#define COLOR_RED     0xC000 

// ==========================================
//        KONFIGURASI HARGA & TAKARAN
// ==========================================
#define NAMA_PRODUK_1  "300 ml"
#define HARGA_PRODUK_1  3000

#define NAMA_PRODUK_2  "400 ml"
#define HARGA_PRODUK_2  4000

#define NAMA_PRODUK_3  "500 ml"
#define HARGA_PRODUK_3  5000
// ==========================================

struct Button {
  int x, y, w, h;
  char label[15];
  uint16_t color;
  int amount;
};

// Sekarang tombol mengambil data dari konfigurasi di atas
Button btn1 = { 30, 100, 260, 75, NAMA_PRODUK_1, COLOR_BLUE,   HARGA_PRODUK_1 };
Button btn2 = { 30, 190, 260, 75, NAMA_PRODUK_2, COLOR_GREEN,  HARGA_PRODUK_2 };
Button btn3 = { 30, 280, 260, 75, NAMA_PRODUK_3, COLOR_ORANGE, HARGA_PRODUK_3 };

Button btnBack = { 30, 410, 260, 50, "BATAL", COLOR_RED, 0 };

void setup() {
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH); 
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0); 
  uint16_t calData[5] = { 444, 2897, 423, 3309, 4 }; 
  tft.setTouch(calData);

  // --- SETUP WIFI MANAGER ---
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 200);
  tft.println("Menghubungkan WiFi...");

  WiFiManager wm;
  // wm.resetSettings(); // Hapus komentar ini jika ingin reset WiFi yang tersimpan
  
  // Jika gagal konek, ESP32 buat AP "WaterVending_AP"
  if (!wm.autoConnect("WaterVending_AP")) {
    tft.println("Gagal! Restarting...");
    delay(3000);
    ESP.restart();
  }

  tft.println("Terhubung!");
  delay(1000);
  drawMenuPage();
}

void loop() {
  uint16_t x = 0, y = 0;
  if (tft.getTouch(&x, &y)) {
    if (currentPage == MENU) {
      if (checkBtn(x, y, btn1)) requestMidtrans(btn1);
      else if (checkBtn(x, y, btn2)) requestMidtrans(btn2);
      else if (checkBtn(x, y, btn3)) requestMidtrans(btn3);
    } 
    else if (currentPage == QR_PAGE) {
      if (checkBtn(x, y, btnBack)) {
        drawMenuPage();
      }
    }
    delay(200); 
  }
}

// --- FUNGSI REQUEST KE MIDTRANS VIA APPSCRIPT ---
void requestMidtrans(Button b) {
  currentPage = LOADING;
  tft.fillScreen(COLOR_BG);
  drawHeader("Proses", "Menghubungkan...");

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); 

    // Susun URL untuk GET: URL?amount=2000&product=500ml
    // Gunakan String(b.label) dan pastikan tidak ada spasi di label (atau gunakan urlencode)
    String urlWithParams = String(appScriptUrl) + "?amount=" + String(b.amount) + "&product=" + String(b.label);
    urlWithParams.replace(" ", "%20"); // Mengatasi spasi pada nama produk

    Serial.println("Requesting URL: " + urlWithParams);

    http.begin(client, urlWithParams);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    // Kirim dengan GET
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response: " + response);

      if (response.indexOf("<html") != -1) {
        showError("Masih Error 400/HTML");
      } else {
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
          // Sesuaikan dengan struktur response Midtrans Anda
          const char* qrUrl = doc["actions"][0]["url"];
          if (qrUrl) {
            openQRPage(b, qrUrl);
          } else {
            showError("QR URL Tidak Ditemukan");
          }
        } else {
          showError("Gagal Baca JSON");
        }
      }
    } else {
      showError("Koneksi Gagal: " + String(httpResponseCode));
    }
    http.end();
  }
}

void openQRPage(Button b, const char* qrLink) {
  currentPage = QR_PAGE;
  tft.fillScreen(COLOR_BG);
  drawHeader("Bayar", "Scan QRIS di bawah:");

  // Info Box
  tft.fillRoundRect(30, 100, 260, 60, 8, 0xF7BE);
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(2);
  tft.setCursor(45, 110);
  tft.print(b.label);
  tft.setCursor(45, 135);
  tft.setTextColor(COLOR_BLUE);
  tft.print("Rp "); tft.print(b.amount);

  generateQRCode(qrLink);
  drawSingleButton(btnBack);
}

// --- FUNGSI UI LAINNYA ---
void drawHeader(const char* title, const char* subtitle) {
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(3);
  tft.setCursor(30, 20);
  tft.print(title);
  tft.drawFastHLine(30, 55, 260, COLOR_HEADER);
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

void drawSingleButton(Button b) {
  tft.fillRoundRect(b.x + 3, b.y + 3, b.w, b.h, 10, 0xD6BA); 
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 10, b.color);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  int textWidth = strlen(b.label) * 18;
  tft.setCursor(b.x + (b.w - textWidth) / 2, b.y + (b.h - 24) / 2);
  tft.print(b.label);
}

void showError(String msg) {
  tft.fillScreen(COLOR_BG);
  drawHeader("Error", "Gagal memuat data");
  tft.setTextColor(COLOR_RED);
  tft.setCursor(30, 150);
  tft.println(msg);
  delay(3000);
  drawMenuPage();
}

bool checkBtn(int tx, int ty, Button b) {
  return (tx > b.x && tx < (b.x + b.w) && ty > b.y && ty < (b.y + b.h));
}

void generateQRCode(const char* msg) {
  uint8_t qrcodeData[qrcode_getBufferSize(5)];
  // Gunakan ECC Low (0) agar pola QR lebih sederhana & mudah discan
  qrcode_initText(&qrcode, qrcodeData, 5, 0, msg);
  
  int scale = 5; // Coba angka 4 jika terlalu besar, atau 5 jika layar cukup luas
  int size = qrcode.size * scale;
  int xOffset = (320 - size) / 2;
  int yOffset = 180; 

  // PENTING: Tambahkan "Quiet Zone" (Frame putih lebih lebar)
  tft.fillRect(xOffset - 15, yOffset - 15, size + 30, size + 30, TFT_WHITE);
  
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        // Gambar blok hitam
        tft.fillRect(x * scale + xOffset, y * scale + yOffset, scale, scale, TFT_BLACK);
      }
    }
  }
}