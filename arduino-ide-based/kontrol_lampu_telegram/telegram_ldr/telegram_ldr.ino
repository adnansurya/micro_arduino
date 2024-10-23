#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Wifi network station credentials
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "1DEAlist"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "1115765927:AAFW2T18s1AcQf7SGgrVChC6mSjhN4YsAgo"
#define TELEGRAM_ID "108488036"

#define ldrPin 34

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

const int batasBawah = 1400;
const int batasAtas = 3000;

String status = "-";
String lastStatus = "-";



void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    bot.sendMessage(bot.messages[i].chat_id, bot.messages[i].text, "");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
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

    bot_lasttime = millis();
  }
  int ldrVal = 4095 - analogRead(ldrPin);

  if (ldrVal <= batasBawah) {
    status = "Gelap";
  } else if (ldrVal > batasBawah && ldrVal <= batasAtas) {
    status = "Redup";
  } else if (ldrVal > batasAtas) {
    status = "Terang";
  } else {
    status = "Error";
  }
  Serial.print("ldrVal: ");
  Serial.print(ldrVal);
  Serial.print("\tStatus: ");
  Serial.print(status);
  Serial.print("\tlastStatus: ");
  Serial.println(lastStatus);



  if (status != lastStatus) {
    String message = "Kondisi " + status;
    bot.sendMessage(TELEGRAM_ID, message, "");
    delay(1000);
  }

  lastStatus = status;
  delay(1000);
}
