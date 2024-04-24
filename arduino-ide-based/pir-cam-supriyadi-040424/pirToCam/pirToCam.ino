
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>


#define pirPin 13
#define ledPin 2
#define outputCamPin 15


// Wifi network station credentials
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "IDEAlist"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "1115765927:AAFgDI003Xn41tererJRuoU543tBsg8CBpE"

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages
String CHAT_ID = "108488036";

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

int adaGerak = 0;
int lastGerak = 0;

void setup() {
  Serial.begin(115200);  // Open serial monitor at 115200 baud to see ping results.

  pinMode(outputCamPin, OUTPUT);
  digitalWrite(outputCamPin, HIGH);  //HIGH = camera standby

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(pirPin, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
}

void loop() {

  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    adaGerak = digitalRead(pirPin);
    // Serial.print("ADA GERAK : ");
    // Serial.print(adaGerak);
    // Serial.println();

    if (adaGerak != lastGerak) {  //filter untuk mendeteksi perubahan status
      if (adaGerak) {
        Serial.println("Gerakan Terdeteksi");
        digitalWrite(outputCamPin, LOW);
        digitalWrite(ledPin, HIGH);
        delay(10);
        digitalWrite(outputCamPin, HIGH);
        digitalWrite(ledPin, LOW);
      }
    }

    lastGerak = adaGerak;

    bot_lasttime = millis();
  }
}


void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    bot.sendMessage(bot.messages[i].chat_id, bot.messages[i].text, "");
  }
}