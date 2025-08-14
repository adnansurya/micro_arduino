#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "secret.h"

#define RELAY_SAKLAR 16
#define RELAY_LAMPU 17
#define offsetHour 8

WiFiClientSecure espClient;
PubSubClient client(espClient);
UniversalTelegramBot bot(BOT_TOKEN, espClient);


// Konfigurasi WiFi
const char* ssid = "Ini namanya";
const char* password = "inipasswordnya";

// Konfigurasi MQTT
const char* mqtt_server = "a9be9b5043a147a2b4a13289f2abdf22.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "jeska";
const char* mqtt_password = "Skripsi123";


// Variabel status
String onTime, offTime;
int onMinute, onHour, offMinute, offHour;
int hour, minute, second;
bool jadwalOnState = false;
bool jadwalOffState = false;
bool lampuState = false;
bool saklarState = false;  // Tambahan variabel status saklar
bool lastLampuState = false;
bool lastSaklarState = false;
bool lastMqttConnected = false;

time_t now;
unsigned long lastMillis = 0;
unsigned long refreshSecondMillis = 1;
unsigned long lastTelegramCheck = 0;
const unsigned long telegramCheckInterval = 1000;


void setup_wifi() {
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi terhubung");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setDateTime() {
  configTime(offsetHour * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < offsetHour * 3600 * 2) {
    delay(100);
    now = time(nullptr);
  }
  Serial.println("Waktu telah disinkronkan.");
}

void sendTelegramNotification(const String &message) {
  if (bot.sendMessage(CHAT_ID, message, "")) {
    Serial.println("Notifikasi Telegram terkirim");
  } else {
    Serial.println("Gagal mengirim notifikasi Telegram");
  }
}

void updateLampuState() {
  now = time(nullptr);
  struct tm* timeInfo = localtime(&now);
  hour = timeInfo->tm_hour;
  minute = timeInfo->tm_min;
  second = timeInfo->tm_sec;

  bool stateChanged = false;
  String changeSource = "";

  // Update status saklar
  saklarState = (digitalRead(RELAY_SAKLAR) == LOW);

  // Logika kontrol lampu
  if (lampuState && jadwalOffState) {
    if (hour == offHour && minute == offMinute && second == 0) {
      lampuState = false;
      stateChanged = true;
      changeSource = "Jadwal";
    }
  }

  if (!lampuState && jadwalOnState) {
    if (hour == onHour && minute == onMinute && second == 0) {
      lampuState = true;
      stateChanged = true;
      changeSource = "Jadwal";
    }
  }

  if (lampuState != lastLampuState) {
    stateChanged = true;
    if (changeSource == "") changeSource = "Remote";
    lastLampuState = lampuState;
  }

  // Kirim notifikasi jika ada perubahan
  if (stateChanged) {
    String notification = "🔄 Status Perangkat:\n";
    notification += "💡 Lampu: " + String(lampuState ? "ON" : "OFF") + "\n";
    notification += "🔌 Saklar: " + String(saklarState ? "ON" : "OFF") + "\n"; // Tambahan status saklar
    notification += "📌 Sumber: " + changeSource + "\n";
    notification += "⏰ Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
    sendTelegramNotification(notification);
  }

  digitalWrite(RELAY_LAMPU, lampuState ? LOW : HIGH);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  Serial.print("Pesan diterima: ");
  Serial.println(messageTemp);

  if (String(topic) == "saklar") {
    bool newSaklarState = (messageTemp == "saklar_on");
    digitalWrite(RELAY_SAKLAR, newSaklarState ? LOW : HIGH);
    
    if (newSaklarState != lastSaklarState) {
      String notification = "🔌 Perubahan Status Saklar:\n";
      notification += "Status: " + String(newSaklarState ? "ON" : "OFF") + "\n";
      notification += "⏰ Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
      sendTelegramNotification(notification);
      lastSaklarState = newSaklarState;
    }
  } 
  else if (String(topic) == "lampu") {
    lampuState = (messageTemp == "lampu_on");
    updateLampuState();
  }
  else if (String(topic) == "lampu/jadwal/on") {
    onTime = messageTemp;
    onHour = onTime.substring(0, 2).toInt();
    onMinute = onTime.substring(3, 5).toInt();
    jadwalOnState = true;
    
    String notification = "⏰ Jadwal ON Diperbarui:\n";
    notification += "Waktu: " + onTime + "\n";
    notification += "Status Saklar: " + String(saklarState ? "ON" : "OFF") + "\n"; // Tambahan status saklar
    sendTelegramNotification(notification);
  }
  else if (String(topic) == "lampu/jadwal/off") {
    offTime = messageTemp;
    offHour = offTime.substring(0, 2).toInt();
    offMinute = offTime.substring(3, 5).toInt();
    jadwalOffState = true;
    
    String notification = "⏰ Jadwal OFF Diperbarui:\n";
    notification += "Waktu: " + offTime + "\n";
    notification += "Status Saklar: " + String(saklarState ? "ON" : "OFF") + "\n"; // Tambahan status saklar
    sendTelegramNotification(notification);
  }
}

void reconnect() {
  if (!client.connected()) {
    if (client.connect("ESP32_Client", mqtt_user, mqtt_password)) {
      client.subscribe("saklar");
      client.subscribe("lampu");
      client.subscribe("lampu/jadwal/on");
      client.subscribe("lampu/jadwal/off");
      
      if (!lastMqttConnected) {
        String notification = "🌐 Koneksi MQTT:\n";
        notification += "Status: Terhubung\n";
        notification += "💡 Lampu: " + String(lampuState ? "ON" : "OFF") + "\n";
        notification += "🔌 Saklar: " + String(saklarState ? "ON" : "OFF") + "\n";
        sendTelegramNotification(notification);
      }
      lastMqttConnected = true;
    } else {
      if (lastMqttConnected) {
        String notification = "⚠️ Koneksi MQTT:\n";
        notification += "Status: Terputus\n";
        notification += "💡 Lampu: " + String(lampuState ? "ON" : "OFF") + "\n";
        notification += "🔌 Saklar: " + String(saklarState ? "ON" : "OFF") + "\n";
        sendTelegramNotification(notification);
      }
      lastMqttConnected = false;
      delay(5000);
    }
  }
}

void checkTelegramMessages() {
  if (millis() - lastTelegramCheck > telegramCheckInterval) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      for (int i = 0; i < numNewMessages; i++) {
        if (String(bot.messages[i].chat_id) != CHAT_ID) {
          bot.sendMessage(bot.messages[i].chat_id, "Unauthorized user", "");
          continue;
        }
        
        String text = bot.messages[i].text;
        if (text == "/status") {
          String statusMessage = "📊 Status Sistem:\n";
          statusMessage += "💡 Lampu: " + String(lampuState ? "ON" : "OFF") + "\n";
          statusMessage += "🔌 Saklar: " + String(saklarState ? "ON" : "OFF") + "\n";
          statusMessage += "⏰ Jadwal ON: " + (jadwalOnState ? onTime : "Tidak aktif") + "\n";
          statusMessage += "⏰ Jadwal OFF: " + (jadwalOffState ? offTime : "Tidak aktif") + "\n";
          statusMessage += "🌐 MQTT: " + String(client.connected() ? "Terhubung" : "Terputus") + "\n";
          statusMessage += "🕒 Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
          bot.sendMessage(CHAT_ID, statusMessage, "");
        }
      }
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTelegramCheck = millis();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_SAKLAR, OUTPUT);
  pinMode(RELAY_LAMPU, OUTPUT);
  digitalWrite(RELAY_SAKLAR, HIGH);
  digitalWrite(RELAY_LAMPU, HIGH);

  setup_wifi();
  setDateTime();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Notifikasi startup
  String notification = "⚡ Sistem Aktif\n";
  notification += "📡 IP: " + WiFi.localIP().toString() + "\n";
  notification += "💡 Lampu: " + String(lampuState ? "ON" : "OFF") + "\n";
  notification += "🔌 Saklar: " + String(saklarState ? "ON" : "OFF") + "\n";
  sendTelegramNotification(notification);
}

void loop() {
   if (!client.connected()) {
    reconnect();
  }
  client.loop();
  updateLampuState();
  // checkTelegramMessages();

  if (millis() - lastMillis > refreshSecondMillis * 1000) {
    Serial.printf("Waktu: %02d:%02d:%02d\n", hour, minute, second);
    Serial.printf("Lampu: %d | Saklar: %d\n", lampuState, saklarState);
    lastMillis = millis();
  }
}