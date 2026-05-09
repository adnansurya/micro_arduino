#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Konfigurasi WiFi Wokwi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Token Bot Telegram
#define BOTtoken "1389983359:AAFQENm7IqedCsbaHiXYOwldBtFuo8_C_ZI"
#define CHAT_ID "108488036"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    if (text == "/on") {
      digitalWrite(2, HIGH);
      bot.sendMessage(CHAT_ID, "Lampu telah DINYALAKAN 💡", "");
      Serial.println("Lampu telah DINYALAKAN");
    }
    if (text == "/off") {
      digitalWrite(2, LOW);
      bot.sendMessage(CHAT_ID, "Lampu telah DIMATIKAN 🌑", "");
      Serial.print("Lampu telah DIMATIKAN");
    }
  }
}

void setup() {
  pinMode(2, OUTPUT);
  WiFi.begin(ssid, password);
  client.setInsecure(); // Sangat penting untuk ESP32/Wokwi agar bisa akses HTTPS
}

void loop() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while(numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}