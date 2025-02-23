// #define CAMERA_MODEL_AI_THINKER

// #include <WiFi.h>
// #include <WiFiClientSecure.h>
// #include "esp_camera.h"
// #include <UniversalTelegramBot.h>
// #include <ArduinoJson.h>
// #include <DHT.h>
// #include <WiFiUdp.h>

// // Definisi pin kamera untuk modul AI Thinker
// #include "camera_pins.h"

// // Definisi pin untuk DHT11
// #define DHTPIN 14
// #define DHTTYPE DHT22

// DHT dht(DHTPIN, DHTTYPE);

// // Definisi pin untuk LED flash
// #define FLASH_LED_PIN 4

// // Ganti dengan kredensial jaringan Anda
// const char* ssid = "Can jie";
// const char* password = "qwerty015";

// // Ganti dengan token bot Telegram Anda
// #define BOTtoken "7714606423:AAE-sFrXKqkawnGxAN3MgWwIyxQgnxW5yHY"

// // Definisi untuk UDP
// const char* udpAddress = "192.168.90.142"; // Ganti dengan IP ESP32 penerima
// const int udpPort = 202018202081; // Port UDP yang digunakan

// WiFiClientSecure client;
// UniversalTelegramBot bot(BOTtoken, client);
// WiFiUDP udp;

// int Bot_mtbs = 1000; // Delay between messages
// long Bot_lasttime;
// bool flashState = LOW;

// camera_fb_t *fb = NULL;
// bool dataAvailable = false;

// bool isMoreDataAvailable();
// byte* getNextBuffer();
// int getNextBufferLen();

// bool setupCamera() {
//   camera_config_t config;
//   config.ledc_channel = LEDC_CHANNEL_0;
//   config.ledc_timer = LEDC_TIMER_0;
//   config.pin_d0 = Y2_GPIO_NUM;
//   config.pin_d1 = Y3_GPIO_NUM;
//   config.pin_d2 = Y4_GPIO_NUM;
//   config.pin_d3 = Y5_GPIO_NUM;
//   config.pin_d4 = Y6_GPIO_NUM;
//   config.pin_d5 = Y7_GPIO_NUM;
//   config.pin_d6 = Y8_GPIO_NUM;
//   config.pin_d7 = Y9_GPIO_NUM;
//   config.pin_xclk = XCLK_GPIO_NUM;
//   config.pin_pclk = PCLK_GPIO_NUM;
//   config.pin_vsync = VSYNC_GPIO_NUM;
//   config.pin_href = HREF_GPIO_NUM;
//   config.pin_sscb_sda = SIOD_GPIO_NUM;
//   config.pin_sscb_scl = SIOC_GPIO_NUM;
//   config.pin_pwdn = PWDN_GPIO_NUM;
//   config.pin_reset = RESET_GPIO_NUM;
//   config.xclk_freq_hz = 20000000;
//   config.pixel_format = PIXFORMAT_JPEG;

//   if (psramFound()) {
//     config.frame_size = FRAMESIZE_VGA;
//     config.jpeg_quality = 10;
//     config.fb_count = 2;
//   } else {
//     config.frame_size = FRAMESIZE_SVGA;
//     config.jpeg_quality = 12;
//     config.fb_count = 1;
//   }

//   // Inisialisasi kamera
//   esp_err_t err = esp_camera_init(&config);
//   if (err != ESP_OK) {
//     Serial.printf("Camera init failed with error 0x%x", err);
//     return false;
//   }
//   return true;
// }

// void handleNewMessages(int numNewMessages) {
//   Serial.println("handleNewMessages");
//   Serial.println(String(numNewMessages));

//   for (int i = 0; i < numNewMessages; i++) {
//     String chat_id = String(bot.messages[i].chat_id);
//     String text = bot.messages[i].text;
//     String from_name = bot.messages[i].from_name;

//     if (from_name == "") from_name = "Guest";

//     if (text == "/flash") {
//       flashState = !flashState;
//       digitalWrite(FLASH_LED_PIN, flashState);
//     }

//     if (text == "/photo") {
//       fb = NULL;
//       fb = esp_camera_fb_get();
//       if (!fb) {
//         Serial.println("Camera capture failed");
//         bot.sendMessage(chat_id, "Camera capture failed", "");
//         return;
//       }
      
//       // Debugging: Log image size
//       Serial.print("Image size: ");
//       Serial.println(fb->len);

//       dataAvailable = true;
//       Serial.println("Sending photo");
//       bool result = bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len,
//                             isMoreDataAvailable, nullptr,
//                             getNextBuffer, getNextBufferLen);

//       if (result) {
//         Serial.println("Photo sent successfully!");
//       } else {
//         Serial.println("Failed to send photo.");
//       }

//       esp_camera_fb_return(fb); 
//     }

//     if (text == "/temperature") {
//       float h = dht.readHumidity();
//       float t = dht.readTemperature();
//       if (isnan(h) || isnan(t)) {
//         bot.sendMessage(chat_id, "Failed to read from DHT sensor!", "");
//       } else {
//         String temperature = "Temperature: " + String(t) + "°C\n";
//         String humidity = "Humidity: " + String(h) + "%\n";
//         bot.sendMessage(chat_id, temperature + humidity, "");
//       }
//     }

//     if (text == "/start") {
//       String welcome = "Welcome to the ESP32Cam Telegram bot.\n\n";
//       welcome += "/photo : take a photo\n";
//       welcome += "/flash : toggle flash LED (VERY BRIGHT!)\n";
//       welcome += "/temperature : get temperature and humidity\n";
//       bot.sendMessage(chat_id, welcome, "Markdown");
//     }
//   }
// }

// bool isMoreDataAvailable() {
//   if (dataAvailable) {
//     dataAvailable = false;
//     return true;
//   } else {
//     return false;
//   }
// }

// byte* getNextBuffer() {
//   if (fb) {
//     return fb->buf;
//   } else {
//     return nullptr;
//   }
// }

// int getNextBufferLen() {
//   if (fb) {
//     return fb->len;
//   } else {
//     return 0;
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   dht.begin();

//   pinMode(FLASH_LED_PIN, OUTPUT);
//   digitalWrite(FLASH_LED_PIN, flashState);

//   if (!setupCamera()) {
//     Serial.println("Camera Setup Failed!");
//     while (true) {
//       delay(100);
//     }
//   }

//   Serial.print("Connecting WiFi: ");
//   Serial.println(ssid);

//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);

//   while (WiFi.status() != WL_CONNECTED) {
//     Serial.print(".");
//     delay(500);
//   }

//   Serial.println("");
//   Serial.println("WiFi connected");
//   Serial.print("IP address: ");
//   Serial.println(WiFi.localIP());

//   bot.longPoll = 60;

//   udp.begin(udpPort); // Mulai UDP untuk pengiriman data suhu
// }

// void loop() {
//   if (millis() > Bot_lasttime + Bot_mtbs) {
//     int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

//     while (numNewMessages) {
//       Serial.println("got response");
//       handleNewMessages(numNewMessages);
//       numNewMessages = bot.getUpdates(bot.last_message_received + 1);
//     }

//     Bot_lasttime = millis();
//   }

//   // Kirim data suhu secara real-time
//   float suhu = dht.readTemperature();
//   float kelembapan = dht.readHumidity();

//   // if (isnan(suhu) || isnan(kelembapan)) {
//   //   Serial.println("Gagal membaca dari sensor DHT!");
//   // } else {
//   //   udp.beginPacket(udpAddress, udpPort);
//   //   udp.print(suhu);
//   //   udp.endPacket();

//   //   Serial.println("Data sent: " + String(suhu));
//   // }

//   delay(5000); // Kirim data setiap 5 detik
// }





#define CAMERA_MODEL_AI_THINKER

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <WiFiUdp.h>

// Definisi pin kamera untuk modul AI Thinker
#include "camera_pins.h"

// Definisi pin untuk DHT22
#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

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

camera_fb_t *fb = NULL;
bool dataAvailable = false;

bool isMoreDataAvailable();
byte* getNextBuffer();
int getNextBufferLen();

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
        } else if (text == "/temperature") {
            float h = dht.readHumidity();
            float t = dht.readTemperature();
            if (isnan(h) || isnan(t)) {
                bot.sendMessage(chat_id, "Failed to read from DHT sensor!", "");
            } else {
                bot.sendMessage(chat_id, "Temperature: " + String(t) + "°C\nHumidity: " + String(h) + "%", "");
            }
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
    dht.begin();
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
    delay(1000);
}
