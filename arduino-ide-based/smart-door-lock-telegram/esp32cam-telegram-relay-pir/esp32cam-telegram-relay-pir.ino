#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ================= ISIKAN DATA ANDA DI SINI =================
const char* ssid = "MIKRO";
const char* password = "1DEAlist1";
#define BOTtoken "8236898914:AAHnrf84yJwaXhTM3gkzV8zv_xJ32cYnFQ8"  // Token dari BotFather
#define CHAT_ID "108488036"                                        // Chat ID dari userinfobot
// ==========================================================

// Definisi Pin GPIO
#define PIR_PIN 13    // Sensor PIR
#define RELAY_PIN 12  // Modul Relay
#define FLASH_PIN 4   // LED Flash bawaan ESP32-CAM

// Model Kamera (AI-THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

bool pirState = false;
bool lastPirState = false;
long last_update_id = 0; // Menyimpan ID Update Telegram terakhir
unsigned long lastBotCheck = 0;
const unsigned long BOT_INTERVAL = 1000; // Cek Telegram setiap 1 detik

// Fungsi Inisialisasi Kamera
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Inisialisasi Kamera Gagal: 0x%x", err);
    ESP.restart();
  }
}

// Fungsi Ambil dan Kirim Foto ke Telegram
void sendPhotoToTelegram() {
  // Nyalakan Flash Sebentar untuk Pencahayaan
  digitalWrite(FLASH_PIN, HIGH);
  delay(200);
  camera_fb_t* fb = esp_camera_fb_get();
  digitalWrite(FLASH_PIN, LOW);

  if (!fb) {
    Serial.println("Gagal mengambil gambar dari kamera!");
    return;
  }

  Serial.println("Mengirim gambar ke Telegram...");

  // Mengirim Foto
  String url = "https://api.telegram.org/bot" + String(BOTtoken) + "/sendPhoto";

  if (client.connect("api.telegram.org", 443)) {
    String head = "--BoundaryString\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(CHAT_ID) + "\r\n--BoundaryString\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--BoundaryString--\r\n";

    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = fb->len + extraLen;

    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: multipart/form-data; boundary=BoundaryString");
    client.println("Content-Length: " + String(totalLen));
    client.println();
    client.print(head);

    uint8_t* fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
      if (n + 1024 < fbLen) {
        client.write(fbBuf + n, 1024);
      } else {
        client.write(fbBuf + n, fbLen - n);
      }
    }
    client.print(tail);
    esp_camera_fb_return(fb);
    Serial.println("Foto berhasil terkirim!");

    // Kirim Tombol Buka Pintu
    String keyboardJson = "[[\"🔓 Buka Pintu\"]]";
    bot.sendMessageWithReplyKeyboard(CHAT_ID, "⚠️ Ada pengunjung terdeteksi di depan pintu!", "", keyboardJson, true);
  } else {
    esp_camera_fb_return(fb);
    Serial.println("Gagal terhubung ke Telegram Server.");
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FLASH_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);  // Pastikan Kunci/Relay dalam kondisi terkunci

  // ================================================================
  // NYALAKAN LED PERTAMA KALI ALAT DIBERI CATU DAYA (BOOTING)
  // ================================================================
  digitalWrite(FLASH_PIN, HIGH);  // LED Flash langsung menyala saat dinyalakan
  delay(500);
  digitalWrite(FLASH_PIN, LOW);

  initCamera();

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  client.setInsecure();  // Wajib dipanggil setelah WiFi terhubung!
  client.setTimeout(15000);

  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");

  // Opsional: Jika ingin LED mati setelah berhasil terhubung ke WiFi,
  // hapus tanda komentar (//) pada baris di bawah ini:
  // digitalWrite(FLASH_PIN, LOW);

  bot.sendMessage(CHAT_ID, "🤖 Sistem Kunci Pintu Otomatis Berhasil Aktif!", "");
}

// Memproses Pesan Telegram Masuk
void handleNewMessages(int numNewMessages) {
  Serial.print("Menerima pesan baru: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Akses ditolak!", "");
      continue;
    }

    String text = bot.messages[i].text;
    Serial.print("Isi pesan: ");
    Serial.println(text);

    if (text == "/start") {
      String welcome = "Sistem Pengunci Pintu Siap.\n\n";
      welcome += "Ketik /buka atau tekan tombol di bawah untuk membuka pintu.";
      String keyboardJson = "[[\"/buka\"]]";
      bot.sendMessageWithReplyKeyboard(CHAT_ID, welcome, "", keyboardJson, true);
    }

    if (text == "/buka") {
      bot.sendMessage(CHAT_ID, "🔓 Membuka pintu selama 5 detik...", "");
      digitalWrite(RELAY_PIN, HIGH); // Aktifkan Solenoid
      delay(5000);                  // Buka pintu selama 5 detik
      digitalWrite(RELAY_PIN, LOW);  // Kunci kembali
      bot.sendMessage(CHAT_ID, "🔒 Pintu telah dikunci kembali.", "");
    }
  }
}

void loop() {
  // 1. Cek Pesan Telegram Secara Berkala
  if (millis() - lastBotCheck > BOT_INTERVAL) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    // Iterasi jika ada pesan baru
    if (numNewMessages > 0) {
      Serial.println("Ada respon dari Telegram!");
      handleNewMessages(numNewMessages);
    }
    
    lastBotCheck = millis();
  }

  // 2. Cek Sensor PIR (Tanpa memblokir loop Telegram)
  pirState = digitalRead(PIR_PIN);
  if (pirState == HIGH && lastPirState == LOW) {
    Serial.println("Gerakan Terdeteksi!");
    sendPhotoToTelegram();
    delay(2000); // Penahanan singkat agar foto tidak dikirim berkali-kali secara instan
  }
  lastPirState = pirState;
}