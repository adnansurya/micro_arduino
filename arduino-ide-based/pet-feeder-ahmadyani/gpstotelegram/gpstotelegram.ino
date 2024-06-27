#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

#define gpsRx 5    //(D1)
#define gpsTx 4    //(D2)
#define buzzer 15  //(D8)

TinyGPSPlus gps;
SoftwareSerial SerialGPS(gpsTx, gpsRx);  //D2 to Tx, D1 to Rx

const char* ssid = "MIKRO";
const char* password = "IDEAlist";

#define BOT_TOKEN "1115765927:AAFW2T18s1AcQf7SGgrVChC6mSjhN4YsAgo"
#define CHAT_ID "108488036"

const unsigned long BOT_MTBS = 3000;
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done


float Latitude, Longitude;
int year, month, date, hour, minute, second;
String DateString, TimeString, LatitudeString, LongitudeString;

bool locationOk = false;
bool spamGPS = false;

unsigned long durasiSpam = 60000;
unsigned long lastSpam;


void setup() {
  pinMode(buzzer, OUTPUT);

  beep(1, 0.5);

  Serial.begin(115200);
  SerialGPS.begin(9600);
  Serial.println();
  Serial.print("Connecting");
  WiFi.begin(ssid, password);

  secured_client.setTrustAnchors(&cert);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  Serial.println("HTTP server started");
  Serial.println(WiFi.localIP());

  bot.sendMessage(CHAT_ID, "GPS Terhubung!", "");
  beep(2, 0.2);
}

void beep(int ulang, float detik) {
  for (int i = 0; i < ulang; i++) {
    digitalWrite(buzzer, HIGH);
    delay(detik * 1000);
    digitalWrite(buzzer, LOW);
    delay(detik * 1000);
  }
}


void loop() {

  if (gps.location.isValid()) {
    
  }

  if (locationOk && spamGPS && millis() > (lastSpam + durasiSpam)) {
    bot.sendMessage(CHAT_ID, htmlBuilder(LatitudeString, LongitudeString), "HTML");
    lastSpam = millis();
  }

  if (LatitudeString != "" && locationOk == false) {
    beep(3, 0.4);
    locationOk = true;
    bot.sendMessage(CHAT_ID, htmlBuilder(LatitudeString, LongitudeString), "HTML");
  }



  while (SerialGPS.available() > 0) {
    if (gps.encode(SerialGPS.read())) {
      if (gps.location.isValid()) {
        Latitude = gps.location.lat();
        LatitudeString = String(Latitude, 6);
        Longitude = gps.location.lng();
        LongitudeString = String(Longitude, 6);
      }

      if (gps.date.isValid()) {
        DateString = "";
        date = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();

        if (date < 10)
          DateString = '0';
        DateString += String(date);

        DateString += " / ";

        if (month < 10)
          DateString += '0';
        DateString += String(month);
        DateString += " / ";

        if (year < 10)
          DateString += '0';
        DateString += String(year);
      }

      if (gps.time.isValid()) {
        TimeString = "";
        hour = gps.time.hour() + 5;  //adjust UTC
        minute = gps.time.minute();
        second = gps.time.second();

        if (hour < 10)
          TimeString = '0';
        TimeString += String(hour);
        TimeString += " : ";

        if (minute < 10)
          TimeString += '0';
        TimeString += String(minute);
        TimeString += " : ";

        if (second < 10)
          TimeString += '0';
        TimeString += String(second);
      }
    }
  }

  if (millis() > bot_lasttime + BOT_MTBS) {
    // Serial.println("scan telegram");
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      beep(1, 0.1);
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}



String htmlBuilder(String lat, String lng) {
  String s = "";
  s += "<b><a href="
       "\"http://maps.google.com/maps?&z=7&mrt=yp&t=k&q=";
  s += lat;
  s += "+";
  s += lng;
  s += "\">Klik Untuk Melihat Lokasi</a></b>";
  return s;
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    String perintah = bot.messages[i].text;
    perintah.trim();
    Serial.println(perintah);
    if (perintah == "/update") {
      if (locationOk) {
        bot.sendMessage(CHAT_ID, "Update Terakhir : " + DateString + ", " + TimeString, "HTML");
        bot.sendMessage(CHAT_ID, htmlBuilder(LatitudeString, LongitudeString), "HTML");        
      } else {
        bot.sendMessage(CHAT_ID, "<b>Lokasi Belum Tersedia</b>", "HTML");
      }
      beep(1, 0.2);
    } else if (perintah == "/dummy") {
      bot.sendMessage(CHAT_ID, htmlBuilder("0.0", "0.0"), "HTML");
      beep(1, 0.2);
    } else if (perintah.indexOf("/spam") > -1) {
      durasiSpam = (long)perintah.substring(6).toInt();

      if (durasiSpam == 0) {
        durasiSpam = 1;
      }

      bot.sendMessage(CHAT_ID, "Lokasi akan dikirimkan setiap " + String(durasiSpam) + " menit", "HTML");
      beep(1, 0.2);
      durasiSpam = durasiSpam * 60000;
      spamGPS = true;
    } else if (perintah == "/mute") {
      spamGPS = false;
      bot.sendMessage(CHAT_ID, "Pengiriman lokasi berkala dimatikan", "HTML");
      beep(1, 0.2);
    } else {
      bot.sendMessage(CHAT_ID, "Perintah tak dikenali", "");
      beep(1, 1);
    }
  }
}
