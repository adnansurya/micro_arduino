#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <base64.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

// Google Apps Script URL
const char* scriptURL = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec";

// Camera pin definitions for AI-Thinker ESP32-CAM
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
#define LED_FLASH 4

WebServer server(80);
WiFiManager wm;

String currentFotoID = "";

void setup() {
  Serial.begin(115200);

  // Initialize LED Flash
  pinMode(LED_FLASH, OUTPUT);
  ledSequence(1, 100);

  // Initialize camera
  if (!initializeCamera()) {
    Serial.println("Camera initialization failed!");
    ledSequence(2, 500);
    while (1)
      ;
  }

  // Connect to WiFi using WiFiManager
  connectWiFi();

  // Setup mDNS
  if (!MDNS.begin("esp32cam")) {
    Serial.println("Error setting up MDNS responder!");
    ledSequence(2, 500);
  }

  // Setup web server routes dengan parameter GET
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);  // Endpoint baru khusus untuk capture
  server.on("/wifi", handleWifiConfig);  // Endpoint untuk konfigurasi WiFi
  server.on("/reset", handleResetWifi);  // Endpoint untuk reset WiFi
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Access via: http://esp32cam.local/capture?foto_id=YOUR_ID");
  Serial.println("WiFi config: http://esp32cam.local/wifi");
  ledSequence(3, 100);
}

void loop() {
  server.handleClient();
  delay(2);
}

bool initializeCamera() {
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

  // Frame parameters
  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }

  return true;
}

void connectWiFi() {
  // Konfigurasi WiFiManager
  wm.setDebugOutput(false); // Nonaktifkan debug output untuk mengurangi spam serial
  wm.setConfigPortalTimeout(180); // Timeout 3 menit untuk config portal
  
  // Custom parameter jika diperlukan
  // WiFiManagerParameter custom_param("param_id", "Param Label", "default", 10);
  // wm.addParameter(&custom_param);

  Serial.println("Menghubungkan ke WiFi...");

  // Coba koneksi ke WiFi yang tersimpan
  if (!wm.autoConnect("ESP32-CAM-AP", "password123")) {
    Serial.println("Gagal terhubung dan timeout terjadi");
    Serial.println("Reset diperlukan atau akses point 'ESP32-CAM-AP' masih tersedia");
    ledSequence(5, 200); // Indikator mode AP
  } else {
    Serial.println("Berhasil terhubung ke WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    ledSequence(2, 200); // Indikator berhasil konek
  }
}

void handleRoot() {
  String html = "<html><head><title>ESP32-CAM Photo Capture</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; }";
  html += "code { background: #f4f4f4; padding: 5px; border-radius: 3px; }";
  html += ".btn { background: #007bff; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; display: inline-block; margin: 5px; }";
  html += ".btn:hover { background: #0056b3; }";
  html += ".btn-danger { background: #dc3545; }";
  html += ".btn-danger:hover { background: #c82333; }";
  html += ".wifi-info { background: #e9ecef; padding: 10px; border-radius: 5px; margin: 10px 0; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32-CAM Photo Capture</h1>";
  
  // Info WiFi
  html += "<div class='wifi-info'>";
  html += "<strong>WiFi Status:</strong> " + String(WiFi.status() == WL_CONNECTED ? "Terhubung" : "Terputus") + "<br>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "<strong>SSID:</strong> " + String(WiFi.SSID()) + "<br>";
    html += "<strong>IP Address:</strong> " + WiFi.localIP().toString();
  }
  html += "</div>";
  
  html += "<p>Gunakan endpoint berikut untuk menangkap foto:</p>";
  html += "<code>http://" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "esp32cam.local") + "/capture?foto_id=ID_ANDA</code>";
  html += "<br><br>";
  html += "<form action='/capture' method='GET'>";
  html += "<strong>Foto ID:</strong> <input type='text' name='foto_id' value='TEST123' required>";
  html += "<input type='submit' value='Capture Photo' class='btn'>";
  html += "</form>";
  html += "<br>";
  html += "<a href='/wifi' class='btn'>Konfigurasi WiFi</a>";
  html += "<a href='/reset' class='btn btn-danger' onclick='return confirm(\"Yakin ingin reset WiFi?\")'>Reset WiFi</a>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleWifiConfig() {
  String html = "<html><head><title>WiFi Configuration</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 5px; }";
  html += ".btn { background: #007bff; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; display: inline-block; }";
  html += ".btn:hover { background: #0056b3; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Konfigurasi WiFi</h1>";
  html += "<p>Device akan restart setelah konfigurasi WiFi.</p>";
  html += "<form method='get' action='/wifisave'>";
  html += "<input type='text' name='s' placeholder='SSID' required><br>";
  html += "<input type='password' name='p' placeholder='Password' required><br>";
  html += "<input type='submit' value='Simpan' class='btn'>";
  html += "</form>";
  html += "<br><a href='/'>Kembali ke Home</a>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleResetWifi() {
  wm.resetSettings();
  String html = "<html><head><title>Reset WiFi</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
  html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>WiFi Reset</h1>";
  html += "<p>Settings WiFi telah direset. Device akan restart dalam 5 detik...</p>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
  delay(5000);
  ESP.restart();
}

// Handler untuk menyimpan konfigurasi WiFi
void handleWifiSave() {
  Serial.println("Menyimpan konfigurasi WiFi...");
  
  String ssid = server.arg("s");
  String password = server.arg("p");
  
  if (ssid.length() > 0) {
    WiFi.begin(ssid.c_str(), password.c_str());
    
    String html = "<html><head><title>WiFi Save</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>Konfigurasi WiFi Disimpan</h1>";
    html += "<p>Mencoba menghubungkan ke: " + ssid + "</p>";
    html += "<p>Device akan restart...</p>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
    
    delay(3000);
    ESP.restart();
  } else {
    server.send(200, "text/plain", "SSID tidak boleh kosong");
  }
}

void handleCapture() {
  // Ambil parameter foto_id dari query string
  currentFotoID = server.arg("foto_id");

  if (currentFotoID.length() > 0) {
    Serial.println("Received Foto ID via GET: " + currentFotoID);

    // Capture photo
    digitalWrite(LED_FLASH, HIGH);
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      server.send(500, "text/plain", "Camera capture failed");
      return;
    }

    // Convert photo to base64 menggunakan library
    String photoBase64 = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb);
    
    Serial.println("Photo captured: " + String(photoBase64.length()) + " characters");
    delay(1000);
    digitalWrite(LED_FLASH, LOW);

    // Send photo to Google Apps Script
    bool success = sendPhotoToGoogleAppsScript(photoBase64, currentFotoID);

    if (success) {
      server.send(200, "text/plain", "Photo captured and uploaded successfully for ID: " + currentFotoID);
    } else {
      server.send(500, "text/plain", "Photo upload failed");
    }
  } else {
    server.send(400, "text/plain", "No Foto ID provided. Usage: /capture?foto_id=YOUR_ID");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

bool sendPhotoToGoogleAppsScript(String imageBase64, String fotoID) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - photo upload failed");
    return false;
  }

  HTTPClient http;
  bool success = false;

  http.begin(scriptURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);  // Timeout 15 detik untuk upload foto

  // Buat JSON payload
  DynamicJsonDocument doc(30000);  // Alokasi lebih besar untuk base64
  doc["action"] = "upload_photo";
  doc["foto_id"] = fotoID;
  doc["image_data"] = imageBase64;

  String payload;
  serializeJson(doc, payload);

  Serial.println("=== Sending Photo to Google Apps Script ===");
  Serial.println("Foto ID: " + fotoID);
  Serial.println("Image data size: " + String(imageBase64.length()) + " chars");
  Serial.println("Payload size: " + String(payload.length()) + " chars");

  int httpCode = http.POST(payload);

  Serial.print("HTTP Response code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Response: " + response);

    // Parse response
    DynamicJsonDocument resDoc(1024);
    deserializeJson(resDoc, response);

    if (resDoc["status"] == "success") {
      Serial.println("✓ Photo uploaded successfully");
      Serial.println("Photo URL: " + resDoc["photo_url"].as<String>());
      success = true;
    } else {
      Serial.println("✗ Photo upload failed on server");
    }
  } else {
    Serial.println("✗ Failed to upload photo");
    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Error response: " + response);
    }
  }

  http.end();
  return success;
}

void ledSequence(int blinks, int duration) { 
  for(int i = 0; i < blinks; i++) {
    digitalWrite(LED_FLASH, HIGH);
    delay(duration);
    digitalWrite(LED_FLASH, LOW);
    if(i < blinks - 1) delay(duration);
  }
}