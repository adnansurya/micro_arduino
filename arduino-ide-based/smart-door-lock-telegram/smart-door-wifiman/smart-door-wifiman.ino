#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ================= KONFIGURASI PIN =================
#define PIR_PIN       13   // Pin Input Sensor PIR
#define RELAY_PIN     12   // Pin Output Relay Solenoid Door Lock
#define FLASH_LED_PIN 4    // Flash LED Bawaan ESP32-CAM

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ================= PIN MAPPING CAMERA (AI-THINKER) =================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ================= VARIABEL GLOBAL & PREFERENCES =================
Preferences preferences;
WiFiClientSecure clientTCP;
UniversalTelegramBot* bot = nullptr;

String SECRET_BOTtoken = "";
String SECRET_CHAT_ID = "";

bool flashState = LOW;
bool sendPhotoFlag = false;

// Variabel Waktu Polling Pesan
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Status PIR Sensor
int lastPirState = LOW;

// ID Pembaruan Telegram (Update ID)
long lastUpdateId = 0;


// ================= INISIALISASI KAMERA =================
void configInitCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count     = 1;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
}

// ================= PENGIRIMAN FOTO =================
String sendPhotoTelegram() {
  flashState = HIGH;
  digitalWrite(FLASH_LED_PIN, flashState);

  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  camera_fb_t* fb = NULL;
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb); // Buang frame awal

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "Camera capture failed";
  }

  delay(300);
  flashState = LOW;
  digitalWrite(FLASH_LED_PIN, flashState);

  if (clientTCP.connect(myDomain, 443)) {
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + SECRET_CHAT_ID + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    size_t imageLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;

    clientTCP.println("POST /bot" + SECRET_BOTtoken + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);

    uint8_t* fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if (n + 1024 < fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      } else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        clientTCP.write(fbBuf, remainder);
      }
    }

    clientTCP.print(tail);
    esp_camera_fb_return(fb);

    int waitTime = 10000;
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + waitTime) > millis()) {
      delay(100);
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state == true) getBody += String(c);
        if (c == '\n') {
          if (getAll.length() == 0) state = true;
          getAll = "";
        } else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length() > 0) break;
    }
    clientTCP.stop();
  } else {
    getBody = "Connected to api.telegram.org failed.";
  }

  return getBody;
}

// ================= EKSEKUSI PERINTAH =================
void processCommand(String chat_id, String text) {
  Serial.print("Menerima perintah: ");
  Serial.println(text);

  if (chat_id != SECRET_CHAT_ID) {
    Serial.println("❌ Chat ID tidak dikenali / ditolak.");
    bot->sendMessage(chat_id, "Unauthorized user", "");
    return;
  }

  if (text == "/start") {
    String welcome = "Sistem Keamanan ESP32-CAM SIAP!\n\n";
    welcome += "Gunakan perintah berikut:\n";
    welcome += "/photo : Ambil foto manual\n";
    welcome += "/unlock : Buka Pintu (Auto Lock 5 detik)\n";
    welcome += "/lock : Kunci Paksa Solenoid\n";
    welcome += "/flash : Toggle Flash LED\n";
    bot->sendMessage(SECRET_CHAT_ID, welcome, "");
  } 
  else if (text == "/unlock") {
    digitalWrite(RELAY_PIN, RELAY_ON);
    bot->sendMessage(SECRET_CHAT_ID, "🔓 Pintu Dibuka! (Akan terkunci otomatis dalam 5 detik)", "");
    Serial.println("Relay ON: Solenoid Unlocked");

    delay(5000);

    digitalWrite(RELAY_PIN, RELAY_OFF);
    bot->sendMessage(SECRET_CHAT_ID, "🔒 Pintu telah Terkunci Kembali otomatis.", "");
    Serial.println("Relay OFF: Solenoid Locked Automatically");
  } 
  else if (text == "/lock") {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    bot->sendMessage(SECRET_CHAT_ID, "🔒 Pintu Dikunci Manual (Solenoid LOCKED)", "");
  } 
  else if (text == "/flash") {
    flashState = !flashState;
    digitalWrite(FLASH_LED_PIN, flashState);
    bot->sendMessage(SECRET_CHAT_ID, "Status Flash diubah", "");
  } 
  else if (text == "/photo") {
    sendPhotoFlag = true;
  }
}

// ================= PENGECEKAN PESAN MANUAL =================
void checkTelegramMessagesManual() {
  if (!clientTCP.connect("api.telegram.org", 443)) {
    Serial.println("Gagal terhubung ke server Telegram untuk cek pesan.");
    return;
  }

  String url = "/bot" + SECRET_BOTtoken + "/getUpdates?offset=" + String(lastUpdateId + 1) + "&timeout=1";
  clientTCP.println("GET " + url + " HTTP/1.1");
  clientTCP.println("Host: api.telegram.org");
  clientTCP.println("Connection: close");
  clientTCP.println();

  String response = "";
  unsigned long timeout = millis();

  while (clientTCP.connected() && millis() - timeout < 3000) {
    while (clientTCP.available()) {
      char c = clientTCP.read();
      response += c;
    }
  }
  clientTCP.stop();

  if (response.indexOf("\"result\":[]") > -1 || response.indexOf("\"update_id\"") == -1) {
    return;
  }

  Serial.println("\n--- Pesan Baru Ditemukan (Raw Response) ---");

  int updateIdIndex = response.lastIndexOf("\"update_id\":");
  if (updateIdIndex != -1) {
    int endUpdateId = response.indexOf(",", updateIdIndex);
    String updateIdStr = response.substring(updateIdIndex + 12, endUpdateId);
    lastUpdateId = updateIdStr.toInt();
  }

  String incomingChatId = "";
  int chatIdIndex = response.lastIndexOf("\"chat\":{\"id\":");
  if (chatIdIndex != -1) {
    int endChatId = response.indexOf(",", chatIdIndex);
    incomingChatId = response.substring(chatIdIndex + 13, endChatId);
  }

  String incomingText = "";
  int textIndex = response.lastIndexOf("\"text\":\"");
  if (textIndex != -1) {
    int endText = response.indexOf("\"", textIndex + 8);
    incomingText = response.substring(textIndex + 8, endText);
  }

  Serial.print("Update ID : "); Serial.println(lastUpdateId);
  Serial.print("Chat ID   : "); Serial.println(incomingChatId);
  Serial.print("Pesan Teks: "); Serial.println(incomingText);

  if (incomingChatId.length() > 0 && incomingText.length() > 0) {
    processCommand(incomingChatId, incomingText);
  }
}

// ================= HELPER LED BLINK =================
void blinkLED(int count, int delayMs) {
  for (int i = 0; i < count; i++) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(delayMs);
  }
}

// ================= SETUP =================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  blinkLED(3, 100); 

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  pinMode(PIR_PIN, INPUT);

  configInitCamera();

  // Load preferences saved previously
  preferences.begin("telegram-cfg", false);
  String savedToken = preferences.getString("bot_token", "");
  String savedChatId = preferences.getString("chat_id", "");

  WiFiManager wm;

  // Buat custom field untuk WiFiManager
  char tokenChar[64];
  char chatIdChar[32];
  savedToken.toCharArray(tokenChar, 64);
  savedChatId.toCharArray(chatIdChar, 32);

  WiFiManagerParameter custom_bot_token("token", "Telegram Bot Token", tokenChar, 64);
  WiFiManagerParameter custom_chat_id("chatid", "Telegram Chat ID", chatIdChar, 32);

  wm.addParameter(&custom_bot_token);
  wm.addParameter(&custom_chat_id);

  // Jalankan WiFiManager Portal
  if (!wm.autoConnect("ESP32-CAM-Config")) {
    Serial.println("Gagal terhubung dan timeout tercapai.");
    delay(3000);
    ESP.restart();
  }

  // Ambil nilai baru jika diubah lewat captive portal
  SECRET_BOTtoken = String(custom_bot_token.getValue());
  SECRET_CHAT_ID = String(custom_chat_id.getValue());

  // Simpan kembali ke Preferences jika ada perubahan
  preferences.putString("bot_token", SECRET_BOTtoken);
  preferences.putString("chat_id", SECRET_CHAT_ID);
  preferences.end();

  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  clientTCP.setTimeout(3000);

  Serial.println("\nWiFi Connected!");
  blinkLED(2, 200);

  configTime(0, 0, "id.pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }

  // Inisialisasi Bot Telegram setelah konfigurasi siap
  bot = new UniversalTelegramBot(SECRET_BOTtoken, clientTCP);
  bot->sendMessage(SECRET_CHAT_ID, "🚨 Sistem Keamanan Aktif & Terhubung via WiFiManager!", "");
}

// ================= LOOP =================
void loop() {
  // 1. Pengecekan Sensor PIR Motion
  int currentPirState = digitalRead(PIR_PIN);
  if (currentPirState == HIGH && lastPirState == LOW) {
    Serial.println("Gerakan Terdeteksi!");
    bot->sendMessage(SECRET_CHAT_ID, "⚠️ PERINGATAN: Gerakan terdeteksi oleh PIR Sensor!", "");
    sendPhotoTelegram();
    lastPirState = HIGH;
  } else if (currentPirState == LOW && lastPirState == HIGH) {
    lastPirState = LOW;
  }

  // 2. Kirim Foto jika ada instruksi manual
  if (sendPhotoFlag) {
    sendPhotoTelegram();
    sendPhotoFlag = false;
  }

  // 3. Pengecekan Pesan Masuk
  if (millis() > lastTimeBotRan + botRequestDelay) {
    checkTelegramMessagesManual();
    lastTimeBotRan = millis();
  }
}