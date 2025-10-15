#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <base64.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

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

  // Connect to WiFi
  connectWiFi();

  // Setup mDNS
  if (!MDNS.begin("esp32cam")) {
    Serial.println("Error setting up MDNS responder!");
    ledSequence(2, 500);
  }

  // Setup web server routes dengan parameter GET
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);  // Endpoint baru khusus untuk capture
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Access via: http://esp32cam.local/capture?foto_id=YOUR_ID");
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
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

void handleRoot() {
  String html = "<html><head><title>ESP32-CAM Photo Capture</title></head><body>";
  html += "<h1>ESP32-CAM Photo Capture</h1>";
  html += "<p>Gunakan endpoint berikut untuk menangkap foto:</p>";
  html += "<code>http://" + WiFi.localIP().toString() + "/capture?foto_id=ID_ANDA</code>";
  html += "<br><br>";
  html += "<form action='/capture' method='GET'>";
  html += "Foto ID: <input type='text' name='foto_id' value='TEST123'>";
  html += "<input type='submit' value='Capture Photo'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
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

String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;

  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }

  return encodedString;
}

void ledSequence(int blinks, int duration) { 
  
  for(int i = 0; i < blinks; i++) {
    digitalWrite(LED_FLASH, HIGH);
    delay(duration);
    digitalWrite(LED_FLASH, LOW);
    if(i < blinks - 1) delay(duration);
  }
  
}