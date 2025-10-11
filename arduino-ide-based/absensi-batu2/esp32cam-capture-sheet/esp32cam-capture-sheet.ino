#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <ESPmDNS.h>

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
#define LED_FLASH         4

WebServer server(80);
const String APPS_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxn7MRT1RSf3AVQ_iqIbfcYe7tNFEL1T5sFIbskY-TS8QcuAuMFW9gxy9P0GFCVuwzA/exec";

// WiFi credentials
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// mDNS hostname
const char* mdns_hostname = "esp32cam";

void setup() {
  Serial.begin(115200);
  Serial.println("Booting ESP32-CAM...");
  
  // Initialize LED Flash
  pinMode(LED_FLASH, OUTPUT);
  digitalWrite(LED_FLASH, LOW);
  
  // Startup LED sequence
  ledSequence(2, 200);
  
  // Initialize Camera
  if (!initCamera()) {
    Serial.println("Camera Init Failed");
    ledSequence(5, 100);
  } else {
    Serial.println("Camera initialized successfully");
    ledSequence(1, 500);
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStartTime < 20000) {
    delay(1000);
    Serial.print(".");
    digitalWrite(LED_FLASH, !digitalRead(LED_FLASH));
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    ledSequence(3, 300);
    
    // Initialize mDNS
    if (!MDNS.begin(mdns_hostname)) {
      Serial.println("Error setting up mDNS responder!");
      ledSequence(4, 200);
    } else {
      Serial.println("mDNS responder started");
      Serial.println("Access via: http://" + String(mdns_hostname) + ".local");
      
      // Add service to mDNS
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS service added: http://esp32cam.local");
      ledSequence(2, 300);
    }
  } else {
    Serial.println("\nWiFi Failed");
    ledSequence(4, 200);
  }
  
  // Setup server routes
  server.on("/api/take_photo", HTTP_POST, handleTakePhoto);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/info", HTTP_GET, handleInfo);
  
  server.begin();
  Serial.println("ESP32-CAM Server Started");
  Serial.println("Ready to receive photo requests from ESP32...");
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

void handleTakePhoto() {
  Serial.println("Photo request received from ESP32...");
  digitalWrite(LED_FLASH, HIGH);
  
  if (server.hasArg("plain")) {
    String jsonData = server.arg("plain");
    
    // Parse JSON data from ESP32
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, jsonData);
    
    if (error) {
      digitalWrite(LED_FLASH, LOW);
      ledSequence(5, 80);
      server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON\"}");
      return;
    }
    
    String cardUID = doc["card_uid"] | "UNKNOWN";
    String timestamp = doc["timestamp"] | "UNKNOWN";
    String rowId = doc["row_id"] | "UNKNOWN";
    String deviceId = doc["device_id"] | "UNKNOWN";
    
    Serial.println("Card UID: " + cardUID);
    Serial.println("Row ID: " + rowId);
    Serial.println("Timestamp: " + timestamp);
    
    // Capture photo
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      digitalWrite(LED_FLASH, LOW);
      ledSequence(3, 500);
      server.send(500, "application/json", "{\"status\":\"error\", \"message\":\"Camera capture failed\"}");
      return;
    }
    
    // Convert photo to base64
    String photoBase64 = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb);
    
    digitalWrite(LED_FLASH, LOW);
    
    Serial.println("Photo captured: " + String(photoBase64.length()) + " characters");
    
    // Send photo to Google Apps Script dengan row ID
    bool success = sendPhotoToGoogleAppsScript(cardUID, timestamp, rowId, photoBase64);
    
    if (success) {
      digitalWrite(LED_FLASH, HIGH);
      delay(800);
      digitalWrite(LED_FLASH, LOW);
      server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"Photo captured and uploaded\"}");
    } else {
      ledSequence(2, 400);
      server.send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to upload photo\"}");
    }
  } else {
    ledSequence(4, 100);
    server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"No data received\"}");
  }
}

bool sendPhotoToGoogleAppsScript(String cardUID, String timestamp, String rowId, String photoBase64) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection");
    return false;
  }
  
  digitalWrite(LED_FLASH, HIGH);
  
  HTTPClient http;
  http.begin(APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(30000); // Timeout lebih lama untuk upload foto
  
  DynamicJsonDocument doc(5000); // Lebih besar untuk foto
  doc["card_uid"] = cardUID;
  doc["timestamp"] = timestamp;
  doc["row_id"] = rowId;
  doc["device_id"] = "ESP32-CAM-01";
  doc["photo"] = photoBase64;
  doc["type"] = "photo_upload";
  doc["photo_status"] = "uploaded";
  
  String payload;
  serializeJson(doc, payload);
  
  Serial.println("Uploading photo to Google Apps Script...");
  
  int httpCode = http.POST(payload);
  bool success = (httpCode == 200);
  
  digitalWrite(LED_FLASH, LOW);
  
  if (success) {
    String response = http.getString();
    Serial.println("Photo uploaded successfully");
    Serial.println("Response: " + response);
    
    delay(300);
    digitalWrite(LED_FLASH, HIGH);
    delay(200);
    digitalWrite(LED_FLASH, LOW);
  } else {
    Serial.println("Failed to upload photo. HTTP Code: " + String(httpCode));
    delay(300);
    ledSequence(2, 200);
  }
  
  http.end();
  return success;
}

void handleHealth() {
  String status = "{";
  status += "\"status\":\"online\",";
  status += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  status += "\"free_heap\":\"" + String(esp_get_free_heap_size()) + "\",";
  status += "\"mdns\":\"http://esp32cam.local\"";
  status += "}";
  
  server.send(200, "application/json", status);
}

void handleInfo() {
  String info = "{";
  info += "\"device\":\"ESP32-CAM\",";
  info += "\"ip_address\":\"" + WiFi.localIP().toString() + "\",";
  info += "\"mac_address\":\"" + WiFi.macAddress() + "\",";
  info += "\"mdns_hostname\":\"esp32cam.local\",";
  info += "\"endpoints\":[\"/api/attendance\", \"/health\", \"/info\"],";
  info += "\"free_heap\":\"" + String(esp_get_free_heap_size()) + "\"";
  info += "}";
  
  server.send(200, "application/json", info);
}

// LED Control Functions
void ledSequence(int blinks, int duration) {
  for(int i = 0; i < blinks; i++) {
    digitalWrite(LED_FLASH, HIGH);
    delay(duration);
    digitalWrite(LED_FLASH, LOW);
    if(i < blinks - 1) delay(duration);
  }
}

void loop() {
  server.handleClient();
  
  // Handle mDNS requests
  // MDNS.update();
  
  delay(10);
}