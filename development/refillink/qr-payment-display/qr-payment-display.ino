#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "qrcode.h"

#define COLOR_BG 0xFFFF
#define COLOR_HEADER 0x2104
#define COLOR_SUB 0x8410
#define COLOR_BLUE 0x2317
#define COLOR_GREEN 0x14A8
#define COLOR_ORANGE 0xE400
#define COLOR_RED 0xC000

// --- KONFIGURASI HARGA ---
#define NAMA_1 "200 ml"
#define HARGA_1 2000
#define NAMA_2 "300 ml"
#define HARGA_2 3000
#define NAMA_3 "400 ml"
#define HARGA_3 4000

const char* appScriptUrl = "https://script.google.com/macros/s/AKfycbyk4k2RuVMENqUIE1uo3RJmTUop-oImhrTKh37as9pRDvWvYMg5viPCgTtGobY5XQdS/exec";  // GANTI URL ANDA

TFT_eSPI tft = TFT_eSPI();
QRCode qrcode;
enum Page { MENU,
            QR_PAGE,
            LOADING,
            SUCCESS };
Page currentPage = MENU;
String currentOrderId = "";
unsigned long lastCheck = 0;

struct Button {
  int x, y, w, h;
  char label[15];
  uint16_t color;
  int amount;
};
Button btn1 = { 30, 100, 260, 75, NAMA_1, 0x2317, HARGA_1 };
Button btn2 = { 30, 190, 260, 75, NAMA_2, 0x14A8, HARGA_2 };
Button btn3 = { 30, 280, 260, 75, NAMA_3, 0xE400, HARGA_3 };
Button btnBack = { 30, 410, 260, 50, "BATAL", 0xC000, 0 };

void setup() {
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  uint16_t calData[5] = { 444, 2897, 423, 3309, 4 };
  tft.setTouch(calData);

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 200);
  tft.print("Connecting WiFi...");

  WiFiManager wm;
  if (!wm.autoConnect("WaterVending_AP")) ESP.restart();

  drawMenuPage();
}

void loop() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    if (currentPage == MENU) {
      if (checkBtn(x, y, btn1)) startTrx(btn1);
      else if (checkBtn(x, y, btn2)) startTrx(btn2);
      else if (checkBtn(x, y, btn3)) startTrx(btn3);
    } else if (currentPage == QR_PAGE && checkBtn(x, y, btnBack)) {
      drawMenuPage();
    }
    delay(200);
  }

  if (currentPage == QR_PAGE && millis() - lastCheck > 3000) {
    lastCheck = millis();
    checkStatus();
  }
}

void startTrx(Button b) {
  currentPage = LOADING;
  tft.fillScreen(COLOR_BG);
  
  // Tampilkan Header dan Pesan Loading yang lebih estetik
  drawHeader("Proses", "Mohon tunggu...");
  
  tft.setTextColor(COLOR_SUB);
  tft.setTextSize(2);
  tft.setCursor(30, 150);
  tft.print("Menghubungkan ke bank");
  
  // Animasi visual sederhana (titik-titik)
  tft.print("."); delay(100); tft.print("."); delay(100); tft.print(".");

  if (WiFi.status() != WL_CONNECTED) {
    showError("WiFi Terputus!");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = String(appScriptUrl) + "?amount=" + String(b.amount) + "&product=" + String(b.label);
  url.replace(" ", "%20");

  http.begin(client, url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000); // Timeout 15 detik

  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    
    // Proteksi jika response bukan JSON (misal error HTML dari Google)
    if (payload.indexOf("<html") != -1) {
      showError("Server Error (400)");
    } else {
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        showError("Data Gagal Dimuat");
      } else {
        // Ambil data QR dan ID
        const char* qr = doc["actions"][0]["url"];
        const char* id = doc["order_id"];
        
        if (qr && id) {
          currentOrderId = String(id);
          showQR(b, qr);
        } else {
          showError("QR Tidak Tersedia");
        }
      }
    }
  } else {
    // Jika httpCode negatif, berarti gagal koneksi (misal -1 atau -11)
    showError("Gagal Server: " + String(httpCode));
  }
  
  http.end();
}

// Fungsi Baru untuk Menampilkan Error Screen
void showError(String msg) {
  tft.fillScreen(COLOR_BG);
  drawHeader("Gagal", "Terjadi kesalahan:");
  
  tft.setTextColor(COLOR_RED);
  tft.setTextSize(2);
  tft.setCursor(30, 120);
  tft.print(msg);
  
  tft.setTextColor(COLOR_SUB);
  tft.setCursor(30, 200);
  tft.print("Kembali ke menu...");
  
  delay(3000); // Beri waktu user membaca pesan
  drawMenuPage();
}

void checkStatus() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, String(appScriptUrl) + "?action=checkStatus&orderId=" + currentOrderId);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (http.GET() > 0) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    String status = doc["transaction_status"];
    
    if (status == "settlement" || status == "capture") {
      currentPage = SUCCESS; // Set state ke sukses
      showSuccessScreen();   // Panggil tampilan sukses yang menarik
      
      // LOGIKA NYALA POMPA
      // digitalWrite(27, LOW); // Contoh: Aktifkan Relay (sesuaikan logic HIGH/LOW)
      
      // Progress Bar Simulasi Air Mengalir
      for (int i = 0; i <= 100; i++) {
        drawProgressBar(i, "Mengalirkan Air...");
        delay(50); // Sesuaikan durasi mengalir di sini
      }
      
      // digitalWrite(27, HIGH); // Matikan Relay
      delay(2000); // Tahan sebentar pesan selesainya
      drawMenuPage();
    }
  }
  http.end();
}


void showSuccessScreen() {
  tft.fillScreen(COLOR_BG);
  
  // 1. Gambar Ikon Centang (Checkmark)
  int centerX = 160;
  int centerY = 150;
  int radius = 40;
  
  // Lingkaran Hijau
  tft.fillCircle(centerX, centerY, radius, COLOR_GREEN);
  tft.fillCircle(centerX, centerY, radius - 4, 0xFFFF); // Outline putih tengah
  tft.fillCircle(centerX, centerY, radius - 8, COLOR_GREEN);
  
  // Simbol Centang Putih (Manual Line)
  tft.drawLine(centerX - 15, centerY, centerX - 5, centerY + 10, 0xFFFF);
  tft.drawLine(centerX - 14, centerY, centerX - 5, centerY + 11, 0xFFFF); // Tebalkan
  tft.drawLine(centerX - 5, centerY + 10, centerX + 20, centerY - 15, 0xFFFF);
  tft.drawLine(centerX - 5, centerY + 11, centerX + 20, centerY - 14, 0xFFFF); // Tebalkan

  // 2. Teks Status
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(3);
  tft.setTextDatum(MC_DATUM); // Set titik acuan ke tengah (Middle Center)
  tft.drawString("PEMBAYARAN", 160, 230);
  tft.setTextColor(COLOR_GREEN);
  tft.drawString("BERHASIL!", 160, 265);
}

void drawProgressBar(int pc, String msg) {
  int barW = 240;
  int barH = 20;
  int barX = (320 - barW) / 2;
  int barY = 320;

  // Gambar Frame Progress
  tft.drawRoundRect(barX - 2, barY - 2, barW + 4, barH + 4, 5, COLOR_SUB);
  
  // Isi Progress
  int progressW = (pc * barW) / 100;
  tft.fillRoundRect(barX, barY, progressW, barH, 3, COLOR_BLUE);
  
  // Teks Informasi di bawah bar
  tft.fillRect(0, barY + 30, 320, 30, COLOR_BG); // Clear area teks sebelumnya
  tft.setTextColor(COLOR_SUB);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(msg, 160, barY + 30);
}

// --- UI FUNCTIONS ---

void drawHeader(const char* title, const char* subtitle) {
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(3);
  tft.setCursor(30, 20);
  tft.print(title);

  tft.drawFastHLine(30, 55, 260, COLOR_HEADER);  // Garis bawah header

  tft.setTextColor(COLOR_SUB);
  tft.setTextSize(2);
  tft.setCursor(30, 65);
  tft.print(subtitle);
}

void drawMenuPage() {
  currentPage = MENU;
  currentOrderId = "";
  tft.fillScreen(0xFFFF);
  drawHeader("Isi Ulang", "Pilih takaran air:");
  // tft.setTextColor(0x2104); tft.setTextSize(3); tft.setCursor(30, 30); tft.print("Pilih Air");
  drawBtn(btn1);
  drawBtn(btn2);
  drawBtn(btn3);
}

void showQR(Button b, const char* url) {
  currentPage = QR_PAGE;
  tft.fillScreen(COLOR_BG);
  drawHeader("Bayar", "Scan QRIS di bawah:");

  // Info Box
  tft.fillRoundRect(30, 100, 260, 60, 8, 0xF7BE);  // Background abu muda
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(2);
  tft.setCursor(45, 110);
  tft.print(b.label);
  tft.setCursor(45, 135);
  tft.setTextColor(COLOR_BLUE);
  tft.print("Rp ");
  tft.print(b.amount);

  // Render QR
  uint8_t qrData[qrcode_getBufferSize(5)];
  qrcode_initText(&qrcode, qrData, 5, 0, url);
  int s = 5, xO = (320 - (qrcode.size * s)) / 2, yO = 180;
  tft.fillRect(xO - 15, yO - 15, (qrcode.size * s) + 30, (qrcode.size * s) + 30, 0xFFFF);
  for (uint8_t y = 0; y < qrcode.size; y++)
    for (uint8_t x = 0; x < qrcode.size; x++)
      if (qrcode_getModule(&qrcode, x, y)) tft.fillRect(x * s + xO, y * s + yO, s, s, 0x0000);

  drawBtn(btnBack);
}

void drawBtn(Button b) {
  // Bayangan tombol (Shadow)
  tft.fillRoundRect(b.x + 3, b.y + 3, b.w, b.h, 10, b.color);
  // Tombol utama
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 10, b.color);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);

  // Center Text Logic
  int textWidth = strlen(b.label) * 18;
  tft.setCursor(b.x + (b.w - textWidth) / 2, b.y + (b.h - 24) / 2);
  tft.print(b.label);
}


bool checkBtn(int tx, int ty, Button b) {
  return (tx > b.x && tx < (b.x + b.w) && ty > b.y && ty < (b.y + b.h));
}