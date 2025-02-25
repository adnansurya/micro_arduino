#define CAMERA_MODEL_AI_THINKER

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

// Definisi pin kamera untuk modul AI Thinker
#include "camera_pins.h"

// Definisi pin untuk LED flash
#define FLASH_LED_PIN 4

// Ganti dengan kredensial jaringan Anda
const char* ssid = "Can jie";
const char* password = "qwerty015";

// Ganti dengan token bot Telegram Anda
#define BOTtoken "7714606423:AAE-sFrXKqkawnGxAN3MgWwIyxQgnxW5yHY"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
WiFiUDP udp;

int Bot_mtbs = 1000;
long Bot_lasttime;
bool flashState = LOW;

camera_fb_t* fb = NULL;
bool dataAvailable = false;

bool isMoreDataAvailable();
byte* getNextBuffer();
int getNextBufferLen();

unsigned long timerSecond = 10;
unsigned long lastSending = 0;

bool setupCamera() {
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
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }
  return true;
}

void handleNewMessages(int numNewMessages) {
  Serial.println("Checking new messages...");
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    Serial.println("Received message: " + text);

    if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      bot.sendMessage(chat_id, "Flash LED toggled!", "");
    } else if (text == "/photo") {
      fb = esp_camera_fb_get();
      if (!fb) {
        bot.sendMessage(chat_id, "Camera capture failed", "");
        return;
      }
      dataAvailable = true;
      bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len, isMoreDataAvailable, nullptr, getNextBuffer, getNextBufferLen);
      esp_camera_fb_return(fb);
    } else if (text == "/start") {
      bot.sendMessage(chat_id, "Welcome to ESP32-CAM Bot!\n/photo - Take a photo\n/flash - Toggle flash LED\n/temperature - Get temperature & humidity", "");
    }
  }
}

bool isMoreDataAvailable() {
  if (dataAvailable) {
    dataAvailable = false;
    return true;
  } else {
    return false;
  }
}

byte* getNextBuffer() {
  return fb ? fb->buf : nullptr;
}

int getNextBufferLen() {
  return fb ? fb->len : 0;
}

void setup() {
  Serial.begin(115200);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);

  if (!setupCamera()) {
    Serial.println("Camera Setup Failed!");
    while (true) {
      delay(100);
    }
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  client.setInsecure();  // Disable SSL certificate validation
  bot.longPoll = 60;
  udp.begin(202018202081);
}

void loop() {
  if (millis() - Bot_lasttime > Bot_mtbs) {
    Serial.println("Checking for new messages...");
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    Bot_lasttime = millis();
  }

  if(millis() > (lastSending + (timerSecond * 1000))){

    sendTimer();

    lastSending = millis();
  }
  
  delay(1000);
}


void sendTimer() {
  udp.beginPacket(udpAddress, udpPort);
  udp.print("send");
  udp.endPacket();
}
