#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

TinyGPSPlus gps;
SoftwareSerial SerialGPS(4, 5);

const char* ssid = "LAGILAGI";
const char* password = "rotibakar";

#define BOT_TOKEN "1389983359:AAHGPAMEwdwmO6Bd_PgtU1cvFPNC65r-oXM"
#define CHAT_ID "108488036"

const unsigned long BOT_MTBS = 3000;
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done


float Latitude, Longitude;
int year, month, date, hour, minute, second;
String DateString, TimeString, LatitudeString, LongitudeString;


ESP8266WebServer server(80);
void setup() {
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


  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
  Serial.println(WiFi.localIP());

  bot.sendMessage(CHAT_ID, "IP Local : " + WiFi.localIP().toString(), "");
}


void loop() {

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
    Serial.println("scan telegram");
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();

  } else {

    server.handleClient();
    MDNS.update();
  }
}

const int led = 13;

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/html", htmlBuilder(true));
  digitalWrite(led, 0);
}

String htmlBuilder(bool forWeb) {
  String s = "";
  if (forWeb) {
    s += "<!DOCTYPE html> <html> <head> <title>NEO-6M GPS Readings</title> <style>";
    s += "table, th, td {border: 1px solid blue;} </style> </head> <body> <h1  style=";
    s += "font-size:300%;";
    s += " ALIGN=CENTER>NEO-6M GPS Readings</h1>";
    s += "<p ALIGN=CENTER style="
         "font-size:150%;"
         "";
    s += "> <b>Location Details</b></p> <table ALIGN=CENTER style=";
    s += "width:50%";
    s += "> <tr> <th>Latitude</th>";
    s += "<td ALIGN=CENTER >";
    s += LatitudeString;
    s += "</td> </tr> <tr> <th>Longitude</th> <td ALIGN=CENTER >";
    s += LongitudeString;
    s += "</td> </tr> <tr>  <th>Date</th> <td ALIGN=CENTER >";
    s += DateString;
    s += "</td></tr> <tr> <th>Time</th> <td ALIGN=CENTER >";
    s += TimeString;
    s += "</td>  </tr> </table> ";
  }

  if (gps.location.isValid()) {
    s += "<p align=center><a style="
         "color:RED;font-size:125%;"
         " href="
         "http://maps.google.com/maps?&z=15&mrt=yp&t=k&q=";
    s += LatitudeString;
    s += "+";
    s += LongitudeString;
    s += ""
         " target="
         "_top"
         ">Click here</a> to open the location in Google Maps.</p>";
  }

  if (forWeb) {
    s += "</body> </html> \n";
  }

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
    Serial.println(perintah);
    if (perintah == "/gps") {
      bot.sendMessage(CHAT_ID, htmlBuilder(false), "");
    } else {
      bot.sendMessage(CHAT_ID, "Perintah tak dikenali", "");
    }
  }
}
