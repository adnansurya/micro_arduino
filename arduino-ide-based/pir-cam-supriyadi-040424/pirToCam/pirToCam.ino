
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>  // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define pirPin 13
#define ledPin 2
#define relayPin 5

// Replace with your network credentials
const char* ssid = "MIKRO";
const char* password = "IDEAlist";

// Initialize Telegram BOT
String BOTtoken = "1115765927:AAFgDI003Xn41tererJRuoU543tBsg8CBpE";  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
String CHAT_ID = "108488036";

#ifdef ESP8266
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

int adaGerak = 0;
int lastGerak = 0;

bool relayState = LOW;
const char* serverName = "http://192.168.188.31/ada_gerakan";

void setup() {
  Serial.begin(115200);  // Open serial monitor at 115200 baud to see ping results.

#ifdef ESP8266
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  client.setTrustAnchors(&cert);     // Add root certificate for api.telegram.org
#endif

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  delay(1000);
  digitalWrite(relayPin, LOW);

  pinMode(pirPin, INPUT);

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

#ifdef ESP32
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    kedipLed(0.2);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());


  kedipLed(2);
}

void loop() {

  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }else{
     adaGerak = digitalRead(pirPin);
  }

 
  // Serial.print("ADA GERAK : ");
  // Serial.print(adaGerak);
  // Serial.println();

  if (adaGerak != lastGerak) {  //filter untuk mendeteksi perubahan status
    if (adaGerak) {
      Serial.println("Gerakan Terdeteksi");
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverName);

        int httpCode = http.GET();
        if (httpCode > 0) {
          String payload = http.getString();
          Serial.println("Response: " + payload);
          // Lakukan sesuatu dengan data yang diterima (misalnya, kendalikan perangkat berdasarkan respons)
        } else {
          Serial.println("Error on HTTP request");
        }

        http.end();
        // delay(5000);  // Tunggu 5 detik sebelum mengirim permintaan berikutnya
      }
      kedipLed(3);
    }
  }

  lastGerak = adaGerak;
  delay(50);
}

void kedipLed(float detik) {
  digitalWrite(ledPin, HIGH);
  delay(detik * 1000);
  digitalWrite(ledPin, LOW);
}

void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
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
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      welcome += "/led_on to turn GPIO ON \n";
      welcome += "/led_off to turn GPIO OFF \n";
      welcome += "/state to request current GPIO state \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/buka") {
      bot.sendMessage(chat_id, "Buka Pintu", "");
      relayState = HIGH;
      digitalWrite(relayPin, relayState);
    }

    if (text == "/kunci") {
      bot.sendMessage(chat_id, "Kunci Pintu", "");
      relayState = LOW;
      digitalWrite(relayPin, relayState);
    }

    if (text == "/cek") {
      if (digitalRead(relayPin) == HIGH) {
        bot.sendMessage(chat_id, "Pintu Terbuka", "");
      } else {
        bot.sendMessage(chat_id, "Pintu Terkunci", "");
      }
    }
  }
}
