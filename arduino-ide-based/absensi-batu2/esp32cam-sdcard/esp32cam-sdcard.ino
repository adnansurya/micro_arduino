#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Camera pin definitions
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

WebServer server(80);
const String APPS_SCRIPT_URL = "https://script.google.com/macros/s/YOUR_SCRIPT_ID/exec";

// WiFi credentials
const char* ssid = "SSID_ANDA";
const char* password = "PASSWORD_ANDA";

void setup() {
  Serial.begin(115200);
  Serial.println("Booting ESP32-CAM...");
  
  // Initialize Camera
  if (!initCamera()) {
    Serial.println("Camera Init Failed");
  } else {
    Serial.println("Camera initialized successfully");
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(1000);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Failed");
  }
  
  // Setup server routes
  server.on("/api/attendance", HTTP_POST, handleAttendance);
  server.on("/health", HTTP_GET, handleHealth);
  
  server.begin();
  Serial.println("ESP32-CAM Server Started");
}

bool initCamera() {
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
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
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

void handleAttendance() {
  Serial.println("Processing attendance request...");
  
  if (server.hasArg("plain")) {
    String jsonData = server.arg("plain");
    
    // Parse JSON data from ESP32
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if (error) {
      server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
      return;
    }
    
    String cardUID = doc["card_uid"] | "UNKNOWN";
    String timestamp = doc["timestamp"] | "UNKNOWN";
    
    Serial.println("Card UID: " + cardUID);
    Serial.println("Timestamp: " + timestamp);
    
    // Capture photo
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      server.send(500, "application/json", "{\"status\":\"error\", \"message\":\"Camera capture failed\"}");
      return;
    }
    
    // Convert photo to base64
    String photoBase64 = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb);
    
    Serial.println("Photo captured: " + String(photoBase64.length()) + " characters");
    
    // Send to Google Apps Script
    bool success = sendToGoogleAppsScript(cardUID, timestamp, photoBase64);
    
    if (success) {
      server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"Photo captured and data sent\"}");
    } else {
      server.send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to send to cloud\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"No data received\"}");
  }
}

bool sendToGoogleAppsScript(String cardUID, String timestamp, String photoBase64) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection");
    return false;
  }
  
  HTTPClient http;
  http.begin(APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  
  DynamicJsonDocument doc(4096);
  doc["card_uid"] = cardUID;
  doc["timestamp"] = timestamp;
  doc["device_id"] = "ESP32-CAM-01";
  doc["photo"] = photoBase64;
  
  String payload;
  serializeJson(doc, payload);
  
  int httpCode = http.POST(payload);
  bool success = (httpCode == 200);
  
  if (success) {
    Serial.println("Data sent to Google Apps Script successfully");
  } else {
    Serial.println("Failed to send to Google Apps Script: " + String(httpCode));
  }
  
  http.end();
  return success;
}

void handleHealth() {
  String status = "{";
  status += "\"status\":\"online\",";
  status += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  status += "\"free_heap\":\"" + String(esp_get_free_heap_size()) + "\"";
  status += "}";
  
  server.send(200, "application/json", status);
}

void loop() {
  server.handleClient();
  delay(10);
}