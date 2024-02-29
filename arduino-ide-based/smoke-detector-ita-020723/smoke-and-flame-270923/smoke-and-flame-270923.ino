#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define WIFI_SSID "Ritaa"
#define WIFI_PASSWORD "wkwkwkwk"

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "6035914260:AAEBnPmOs_TGL9RGCHpwAoAJiNwjDCpKW7U"

#define mq7Pin 33
#define flamePin 26


#define buzzerPin 25
#define fanPin 32

int adcSmoke = 0;


const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

String idTujuan = "5707679049";  //"108488036";
unsigned long bot_lasttime;      // last time messages' scan has been done

void setup() {
  Serial.begin(9600);

  pinMode(buzzerPin, OUTPUT);
  beep(buzzerPin, 1, 0.2);

  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, HIGH);

  pinMode(flamePin, INPUT);

  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  beep(buzzerPin, 3, 1);

  Serial.print("WiFi connected. IP address: ");
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
  delay(1500);
}

void loop() {
  adcSmoke = analogRead(mq7Pin);
  int persenSmoke = map(adcSmoke, 0, 4095, 0, 100);
  Serial.print("MQ : ");
  Serial.print(adcSmoke);
  Serial.print(" adc \t| ");
  Serial.print(persenSmoke);
  Serial.println(" %");

  int adaApi = !digitalRead(flamePin);
  Serial.print("API: ");
  Serial.println(adaApi);


  if (persenSmoke > 10) {
    bot.sendMessage(idTujuan, "Asap melebihi 10% !", "");
    kondisiBahaya();
  } else if (adaApi) {
    bot.sendMessage(idTujuan, "Api Terdeteksi", "");
    kondisiBahaya();
  } else {
    kondisiAman();
  }
  delay(500);
}


void beep(int kodePin, int berapaKali, float delayDetik) {
  for (int i = 0; i < berapaKali; i++) {
    digitalWrite(kodePin, HIGH);
    delay(delayDetik * 1000);
    digitalWrite(kodePin, LOW);
    delay(delayDetik * 1000);
  }
}

void kondisiAman() {
  digitalWrite(buzzerPin, LOW);
  digitalWrite(fanPin, HIGH);
}

void kondisiBahaya() {
  digitalWrite(buzzerPin, HIGH);
  digitalWrite(fanPin, LOW);
}
