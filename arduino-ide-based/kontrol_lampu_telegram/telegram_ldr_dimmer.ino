#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <dimmable_light.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "ACS712.h"

const int syncPin = 13;
const int thyristorPin = 14;



// Wifi network station credentials
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "1DEAlist"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "7379070072:AAFVAhyhxdTZpRrFsZIhNrueuLXWSS5_gqg"
#define TELEGRAM_ID "5270733104"

#define GMT_OFFSET 8

#define ldrPin 34
#define syncPin 13
#define thyristorPin 14

#define acsPin 32

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

DimmableLight light(thyristorPin);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

ACS712 sensor(ACS712_05B, acsPin);

const int batasBawah = 1400;
const int batasAtas = 3000;

const int jamMalam[] = { 18, 0 };
const int jamPagi[] = { 6, 0 };

String status = "-";
String lastStatus = "-";
String statLampu = "-";
String lastLampu = "-";

int lampuTerang = 250;
int lampuRedup = 128;
int lampuMati = 0;

int lampuOut = 0;

int ldrVal = 0;

bool isMalam = false;

float batasArus = 0.06;

int currentMinute, currentHour, currentSecond;

bool putus = false;

float I;



void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String command = bot.messages[i].text;
    if (command == "/status") {
      String msg = "Status LDR : " + status + "\nLdr Value : " + ldrVal + "\nLampu " + statLampu + "\nArus : " + I + " A";

      bot.sendMessage(bot.messages[i].chat_id, msg, "");
    }
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

  Serial.print("Initializing DimmableLight library... ");
  DimmableLight::setSyncPin(syncPin);
  // VERY IMPORTANT: Call this method to activate the library
  DimmableLight::begin();
  Serial.println("Done!");

  timeClient.begin();
  timeClient.setTimeOffset(GMT_OFFSET * 3600);

  sensor.calibrate();
}

void loop() {
  if (millis() - bot_lasttime > BOT_MTBS) {
    timeClient.update();
    currentHour = timeClient.getHours();
    currentMinute = timeClient.getMinutes();
    currentSecond = timeClient.getSeconds();

    Serial.print(currentHour);
    Serial.print(":");
    Serial.print(currentMinute);
    Serial.print(":");
    Serial.println(currentSecond);


    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
  ldrVal = 4095 - analogRead(ldrPin);

  if (ldrVal <= batasBawah) {
    status = "Gelap";
    if (putus) {
      statLampu = "Putus";
    } else {
      statLampu = "Terang";
    }

    lampuOut = lampuTerang;
  } else if (ldrVal > batasBawah && ldrVal <= batasAtas) {
    status = "Redup";
    if (putus) {
      statLampu = "Putus";
    } else {
      statLampu = "Redup";
    }
    lampuOut = lampuRedup;
  } else if (ldrVal > batasAtas) {
    status = "Terang";
    if (putus) {
      statLampu = "Putus";
    } else {
      statLampu = "Mati";
    }
    lampuOut = lampuMati;
  } else {
    status = "Error";
  }
  Serial.print("ldrVal: ");
  Serial.print(ldrVal);
  Serial.print("\tStatus: ");
  Serial.print(status);
  Serial.print("\tlastStatus: ");
  Serial.println(lastStatus);


  if (currentHour == jamMalam[0] && currentMinute >= jamMalam[1]) {
    lampuOut = lampuTerang;
    if (putus) {
      statLampu = "Putus";
    } else {
      statLampu = "Terang";
    }

    if (isMalam == false) {

      String message = "Memasuki Waktu Malam";
      bot.sendMessage(TELEGRAM_ID, message, "");
      delay(1000);
      isMalam = true;
    }
  }

  if (currentHour == jamPagi[0] && currentMinute >= jamPagi[1]) {
    lampuOut = lampuMati;
    if (putus) {
      statLampu = "Putus";
    } else {
      statLampu = "Mati";
    }

    if (isMalam == true) {
      String message = "Memasuki Waktu Pagi";
      bot.sendMessage(TELEGRAM_ID, message, "");
      delay(1000);
      isMalam = false;
    }
  }

  light.setBrightness(lampuOut);

  if (status != lastStatus || statLampu != lastLampu) {
    String message = "Kondisi " + status + "\nLampu " + statLampu;
    bot.sendMessage(TELEGRAM_ID, message, "");
    delay(1000);
  }

  if (statLampu != "Mati") {
    cekLampu();
  }

  lastStatus = status;
  lastLampu = statLampu;
  delay(1000);
}

void cekLampu() {
  I = sensor.getCurrentAC();
  Serial.print("Arus : ");
  Serial.print(I);
  Serial.println(" A");

  if (I <= batasArus) {
    if (putus == false) {
      String message = "Lampu Terindikasi Putus! \nArus : " + String(I) + " A";
      bot.sendMessage(TELEGRAM_ID, message, "");
      delay(1000);
    }

    putus = true;
  } else {
    if (putus == true) {
      String message = "Lampu Menyala Kembali! \nArus : " + String(I) + " A";
      bot.sendMessage(TELEGRAM_ID, message, "");
      delay(1000);
    }
    putus = false;
  }
}