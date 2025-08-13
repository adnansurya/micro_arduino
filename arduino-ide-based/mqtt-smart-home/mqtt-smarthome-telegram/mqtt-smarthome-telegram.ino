#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "secret.h"

#define RELAY_SAKLAR 16  // Relay untuk saklar
#define RELAY_LAMPU 17   // Relay untuk lampu

#define offsetHour 8

// Konfigurasi WiFi
const char* ssid = "Ini namanya";
const char* password = "inipasswordnya";

// Konfigurasi MQTT
const char* mqtt_server = "a9be9b5043a147a2b4a13289f2abdf22.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "jeska";
const char* mqtt_password = "Skripsi123";


WiFiClientSecure espClient;
PubSubClient client(espClient);
UniversalTelegramBot bot(BOT_TOKEN, espClient);

String onTime, offTime;
int onMinute, onHour, offMinute, offHour;
int hour, minute, second;
bool jadwalOnState = false;
bool jadwalOffState = false;
bool lampuState = false;
bool lastLampuState = false;
bool lastSaklarState = false;
bool lastMqttConnected = false; // Untuk memantau status koneksi MQTT sebelumnya

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
  Serial.print("Menunggu sinkronisasi waktu NTP...");
  configTime(offsetHour * 3600, 0, "pool.ntp.org", "time.nist.gov");

  now = time(nullptr);

  while (now < offsetHour * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println("\nWaktu telah disinkronkan.");
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

  if (lampuState && jadwalOffState) {
    if (hour == offHour && minute == offMinute && second == 0) {
      Serial.println("OFF by Jadwal");
      lampuState = false;
      stateChanged = true;
      changeSource = "Jadwal";
    }
  }

  if (!lampuState && jadwalOnState) {
    if (hour == onHour && minute == onMinute && second == 0) {
      Serial.println("ON by Jadwal");
      lampuState = true;
      stateChanged = true;
      changeSource = "Jadwal";
    }
  }

  if (lampuState != lastLampuState) {
    stateChanged = true;
    if (changeSource == "") {
      changeSource = "Remote";
    }
    lastLampuState = lampuState;
  }

  if (stateChanged) {
    String notification = "Status Lampu berubah:\n";
    notification += "Status: " + String(lampuState ? "ON" : "OFF") + "\n";
    notification += "Sumber: " + changeSource + "\n";
    notification += "Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
    sendTelegramNotification(notification);
  }

  if (lampuState) {
    digitalWrite(RELAY_LAMPU, LOW);
  } else {
    digitalWrite(RELAY_LAMPU, HIGH);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  Serial.print("Pesan diterima dari topik: ");
  Serial.println(topic);
  Serial.print("Pesan: ");
  Serial.println(messageTemp);

  if (String(topic) == "saklar") {
    bool newSaklarState = (messageTemp == "saklar_on");
    digitalWrite(RELAY_SAKLAR, newSaklarState ? LOW : HIGH);
    
    if (newSaklarState != lastSaklarState) {
      String notification = "Status Saklar berubah:\n";
      notification += "Status: " + String(newSaklarState ? "ON" : "OFF") + "\n";
      notification += "Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
      sendTelegramNotification(notification);
      lastSaklarState = newSaklarState;
    }
    
  } else if (String(topic) == "lampu") {
    lampuState = (messageTemp == "lampu_on");
    updateLampuState();

  } else if (String(topic) == "lampu/jadwal/on") {
    onTime = messageTemp.c_str();
    onHour = onTime.substring(0, 2).toInt();
    onMinute = onTime.substring(3, 5).toInt();
    jadwalOnState = true;
    updateLampuState();

    String notification = "Jadwal ON diperbarui:\n";
    notification += "Waktu: " + onTime + "\n";
    notification += "Waktu server: " + String(hour) + ":" + String(minute) + ":" + String(second);
    sendTelegramNotification(notification);

  } else if (String(topic) == "lampu/jadwal/off") {
    offTime = messageTemp.c_str();
    offHour = offTime.substring(0, 2).toInt();
    offMinute = offTime.substring(3, 5).toInt();
    jadwalOffState = true;
    updateLampuState();

    String notification = "Jadwal OFF diperbarui:\n";
    notification += "Waktu: " + offTime + "\n";
    notification += "Waktu server: " + String(hour) + ":" + String(minute) + ":" + String(second);
    sendTelegramNotification(notification);
  }
}

void reconnect() {
  static bool connectionStatusChanged = false;
  
  if (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT... ");
    
    if (client.connect("ESP32_Client", mqtt_user, mqtt_password)) {
      Serial.println("Berhasil terhubung!");
      client.subscribe("saklar");
      client.subscribe("lampu");
      client.subscribe("lampu/jadwal/on");
      client.subscribe("lampu/jadwal/off");
      
      // Hanya kirim notifikasi jika sebelumnya tidak terhubung
      if (!lastMqttConnected) {
        String notification = "✅ Terhubung ke MQTT broker\n";
        notification += "Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
        sendTelegramNotification(notification);
      }
      lastMqttConnected = true;
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik...");
      
      // Hanya kirim notifikasi jika sebelumnya terhubung
      if (lastMqttConnected) {
        String notification = "⚠️ Terputus dari MQTT broker\n";
        notification += "Kode error: " + String(client.state()) + "\n";
        notification += "Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
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
      Serial.println("Menerima pesan Telegram");
      for (int i = 0; i < numNewMessages; i++) {
        String chat_id = String(bot.messages[i].chat_id);
        if (chat_id != CHAT_ID) {
          bot.sendMessage(chat_id, "Unauthorized user", "");
          continue;
        }
        
        String text = bot.messages[i].text;
        String from_name = bot.messages[i].from_name;

        if (text == "/status") {
          String statusMessage = "📊 Status Sistem:\n";
          statusMessage += "💡 Lampu: " + String(lampuState ? "ON" : "OFF") + "\n";
          statusMessage += "🔌 Saklar: " + String(digitalRead(RELAY_SAKLAR) == LOW ? "ON" : "OFF") + "\n";
          statusMessage += "⏰ Jadwal ON: " + (jadwalOnState ? (onTime + " ✅") : "❌ Tidak aktif") + "\n";
          statusMessage += "⏰ Jadwal OFF: " + (jadwalOffState ? (offTime + " ✅") : "❌ Tidak aktif") + "\n";
          statusMessage += "📶 Status MQTT: " + String(client.connected() ? "✅ Terhubung" : "❌ Terputus") + "\n";
          statusMessage += "🕒 Waktu Sistem: " + String(hour) + ":" + String(minute) + ":" + String(second);
          bot.sendMessage(chat_id, statusMessage, "");
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

  setup_wifi();
  setDateTime();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  digitalWrite(RELAY_SAKLAR, HIGH);
  digitalWrite(RELAY_LAMPU, HIGH);
  lastSaklarState = false;
  lastLampuState = false;
  lastMqttConnected = false;

  // Notifikasi startup hanya sekali
  String notification = "⚡ Sistem telah dihidupkan\n";
  notification += "📡 Alamat IP: " + WiFi.localIP().toString() + "\n";
  notification += "🕒 Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
  sendTelegramNotification(notification);
}

void loop() {
  reconnect(); // Fungsi reconnect sekarang menangani notifikasi status koneksi
  client.loop();

  updateLampuState();
  checkTelegramMessages();

  if (millis() > (refreshSecondMillis * 1000) + lastMillis) {
    Serial.printf("Waktu:  %02d:%02d:%02d\n", hour, minute, second);

    Serial.print("LampuState: ");
    Serial.print(lampuState);
    Serial.print("\tJadwalOn: ");
    Serial.print(jadwalOnState);
    Serial.print("\tJadwalOff: ");
    Serial.println(jadwalOffState);

    lastMillis = millis();
  }
}