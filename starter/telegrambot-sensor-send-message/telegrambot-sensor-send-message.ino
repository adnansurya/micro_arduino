#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Wifi network station credentials
#define WIFI_SSID "WARKOP LATEMMAMALA"
#define WIFI_PASSWORD "OKTOBER10"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "8275612758:AAENe80DiG1ft0ijjbCbjuOQb7tVMgX2F3c"

int pinSensor = D5;

String chatId = "7917806728";


X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(pinSensor, INPUT);

  
  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
}

void loop() {
  // put your main code here, to run repeatedly:
  int nilaiSensor = digitalRead(pinSensor);
  Serial.print("TERBACA : ");
  Serial.println(nilaiSensor);
  if(nilaiSensor == 1){
    Serial.println("Anomali");
    bot.sendMessage(chatId, "BOMBER, BOMBER", "");
  }else{
    Serial.println("kalem");
  }
  delay(1000);

}
