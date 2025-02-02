#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#define PIR_PIN 13
#define FLASH_LED_PIN 4

// Konfigurasi WiFi
#define WIFI_SSID "MIKRO"         // Ganti dengan SSID WiFi Anda
#define WIFI_PASSWORD "1DEAlist"  // Ganti dengan password WiFi Anda

#define GMT_OFFSET_HOUR 8

String BOTtoken = "1389983359:AAH9kWOMCYhD15psuYa14Ci5KOhBXHzEGCM";  // your Bot Token (Get from Botfather)
String CHAT_ID = "108488036";

WiFiClientSecure clientTCP, ssl;
UniversalTelegramBot bot(BOTtoken, clientTCP);

//Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

bool sendPhoto = false;
bool flashState = LOW;

#define DATABASE_URL "https://skripsi-c7455-default-rtdb.asia-southeast1.firebasedatabase.app/"

String dirPath = "/logs";
time_t tstamp;

DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
NoAuth noAuth;

int v, lastV;

//CAMERA_MODEL_AI_THINKER
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

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  // Init Serial Monitor
  Serial.begin(115200);

  // Set LED Flash as output
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);

  // Config and init the camera
  configInitCamera();

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // ledBlink(2, 100);
  Serial.println();

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  tstamp = getTimestamp();

  String ipNotif = "ESP32 Cam IP Server : " + WiFi.localIP().toString();
  bot.sendMessage(CHAT_ID, ipNotif, "");

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  ssl.setInsecure();
#if defined(ESP8266)
  ssl.setBufferSizes(1024, 1024);
#endif

  // Initialize the authentication handler.
  initializeApp(client, app, getAuth(noAuth));

  // Binding the authentication handler with your Database class object.
  app.getApp<RealtimeDatabase>(Database);

  // Set your database URL
  Database.url(DATABASE_URL);

  // In sync functions, we have to set the operating result for the client that works with the function.
  client.setAsyncResult(result);

  ledBlink(2, 200);
}


time_t getTimestamp() {
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  return now;
}



void loop() {

  if (sendPhoto) {

    Serial.println("Preparing photo");
    sendPhotoTelegram();
    sendPhoto = false;
    digitalWrite(FLASH_LED_PIN, LOW);
  }
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  pinMode(PIR_PIN, INPUT_PULLUP);
  v = digitalRead(PIR_PIN);
  Serial.println(v);
  if (v == 1 && lastV != v) {
    sendPhoto = true;
  }

  lastV = v;
  delay(20);
}


void sendToFirebase(camera_fb_t* fb) {

  // Konversi gambar ke Base64
  String imageBase64 = base64::encode((uint8_t*)fb->buf, fb->len);
  // Serial.println(imageBase64);

  String pushId = getPushID(dirPath);

  String imageDir = dirPath + "/" + pushId + "/photo";
  Serial.print("Set string... ");
  bool status = Database.set<String>(client, imageDir, imageBase64);
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  tstamp = getTimestamp();
  long timeInt = (long)tstamp;

  String timeDir = dirPath + "/" + pushId + "/timestamp";
  Serial.print("Set int... ");
  status = Database.set<int>(client, timeDir, timeInt);
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());

  String datetime = convertTimestampToDateTime(timeInt, GMT_OFFSET_HOUR);
  String datetimeDir = dirPath + "/" + pushId + "/datetime";
  Serial.print("Set string... ");
  status = Database.set<String>(client, datetimeDir, datetime);
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());
  
}

String getPushID(String path) {

  String pID = "";

  Serial.print("getPushId... ");
  String name = Database.push<int>(client, path, 0);
  if (client.lastError().code() == 0) {
    Firebase.printf("ok, name: %s\n", name.c_str());
    pID = name;
  } else {
    printError(client.lastError().code(), client.lastError().message());
  }

  return pID;
}


void configInitCamera() {
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
  config.grab_mode = CAMERA_GRAB_LATEST;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;  //0-63 lower number means higher quality
  config.fb_count = 1;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t* s = esp_camera_sensor_get();

  // Mengubah orientasi gambar
  // s->set_hmirror(s, 1);  // Mengaktifkan horizontal mirror
  // s->set_vflip(s, 1);  // Mengaktifkan vertical flip
}

void handleNewMessages(int numNewMessages) {
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;
    if (text == "/start") {
      String welcome = "Welcome , " + from_name + "\n";
      welcome += "Use the following commands to interact with the ESP32-CAM \n";
      welcome += "/photo : takes a new photo\n";
      welcome += "/flash : toggles flash LED \n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      Serial.println("Change flash LED state");
    }
    if (text == "/photo") {
      sendPhoto = true;
      Serial.println("New photo request");
    }
  }
}

String sendPhotoTelegram() {

  digitalWrite(FLASH_LED_PIN, HIGH);

  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

  //Dispose first picture because of bad quality
  camera_fb_t* fb = NULL;
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // dispose the buffered image

  // Take a new photo
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }

  digitalWrite(FLASH_LED_PIN, LOW);
  flashState = LOW;

  sendToFirebase(fb);

  delay(500);

  Serial.println("Connect to " + String(myDomain));


  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");

    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    size_t imageLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;

    clientTCP.println("POST /bot" + BOTtoken + "/sendPhoto HTTP/1.1");
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

    int waitTime = 10000;  // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + waitTime) > millis()) {
      Serial.print(".");
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
    Serial.println(getBody);
  } else {
    getBody = "Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }

  return getBody;
}

void ledBlink(int freq, int delayInterval) {
  for (int i = 0; i < freq; i++) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(delayInterval);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(delayInterval);
  }
  flashState = LOW;
}

// Fungsi untuk mengonversi timestamp ke format tanggal dan waktu
String convertTimestampToDateTime(unsigned long timestamp, long offsetHour) {
    struct tm timeinfo;
    time_t rawtime = (time_t) (timestamp + (offsetHour * 3600));

    // Konversi timestamp ke struct tm
    localtime_r(&rawtime, &timeinfo);

    // Format hasil menjadi string (YYYY-MM-DD HH:MM:SS)
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    
    return String(buffer);
}

void printError(int code, const String& msg) {
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}