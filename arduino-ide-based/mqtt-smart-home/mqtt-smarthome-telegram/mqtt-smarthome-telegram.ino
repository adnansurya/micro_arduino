#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

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

// Konfigurasi Telegram Bot
#define BOT_TOKEN "1837465469:AAHGQzX5EzMhAGCKkHS8IiBvEJJ5t1e6O8c"
#define CHAT_ID "108488036"

WiFiClientSecure espClient;
PubSubClient client(espClient);
UniversalTelegramBot bot(BOT_TOKEN, espClient);

String onTime, offTime;
int onMinute, onHour, offMinute, offHour;
int hour, minute, second;
bool jadwalOnState = false;
bool jadwalOffState = false;
bool lampuState = false;  // Variabel untuk menyimpan state lampu
bool lastLampuState = false; // Untuk memantau perubahan state lampu
bool lastSaklarState = false; // Untuk memantau perubahan state saklar

time_t now;
unsigned long lastMillis = 0;
unsigned long refreshSecondMillis = 1;
unsigned long lastTelegramCheck = 0;
const unsigned long telegramCheckInterval = 1000; // Periksa pesan Telegram setiap 1 detik

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
  configTime(offsetHour * 3600, 0, "pool.ntp.org", "time.nist.gov");  // Waktu WIB (UTC+8)

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
  // Lampu menyala jika dalam rentang waktu onTime dan offTime
  now = time(nullptr);

  // Konversi ke struktur waktu lokal
  struct tm* timeInfo = localtime(&now);

  // Ambil jam dan menit
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

  // Periksa perubahan state lampu
  if (lampuState != lastLampuState) {
    stateChanged = true;
    if (changeSource == "") {
      changeSource = "Remote";
    }
    lastLampuState = lampuState;
  }

  // Kirim notifikasi jika ada perubahan state
  if (stateChanged) {
    String notification = "Status Lampu berubah:\n";
    notification += "Status: " + String(lampuState ? "ON" : "OFF") + "\n";
    notification += "Sumber: " + changeSource + "\n";
    notification += "Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
    sendTelegramNotification(notification);
  }

  if (lampuState) {
    digitalWrite(RELAY_LAMPU, LOW);  // Menyalakan relay
  } else {
    digitalWrite(RELAY_LAMPU, HIGH);  // Mematikan relay
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
    
    // Kirim notifikasi jika ada perubahan state saklar
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

    // Kirim notifikasi jadwal baru
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

    // Kirim notifikasi jadwal baru
    String notification = "Jadwal OFF diperbarui:\n";
    notification += "Waktu: " + offTime + "\n";
    notification += "Waktu server: " + String(hour) + ":" + String(minute) + ":" + String(second);
    sendTelegramNotification(notification);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT... ");

    if (client.connect("ESP32_Client", mqtt_user, mqtt_password)) {
      Serial.println("Berhasil terhubung!");
      client.subscribe("saklar");
      client.subscribe("lampu");
      client.subscribe("lampu/jadwal/on");
      client.subscribe("lampu/jadwal/off");
      
      // Kirim notifikasi saat terhubung ke MQTT
      String notification = "Sistem terhubung ke MQTT broker\n";
      notification += "Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
      sendTelegramNotification(notification);
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik...");
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
          String statusMessage = "Status Sistem:\n";
          statusMessage += "Saklar: " + String(digitalRead(RELAY_SAKLAR) == LOW ? "ON" : "OFF") + "\n";
          statusMessage += "Lampu: " + String(lampuState ? "ON" : "OFF") + "\n";
          statusMessage += "Jadwal ON: " + (jadwalOnState ? (onTime + " Aktif") : "Tidak aktif") + "\n";
          statusMessage += "Jadwal OFF: " + (jadwalOffState ? (offTime + " Aktif") : "Tidak aktif") + "\n";
          statusMessage += "Waktu Sistem: " + String(hour) + ":" + String(minute) + ":" + String(second);
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

  // Memastikan relay mati saat memulai
  digitalWrite(RELAY_SAKLAR, HIGH);
  digitalWrite(RELAY_LAMPU, HIGH);
  lastSaklarState = false;
  lastLampuState = false;

  // Kirim notifikasi startup
  String notification = "Sistem telah dihidupkan\n";
  notification += "Alamat IP: " + WiFi.localIP().toString() + "\n";
  notification += "Waktu: " + String(hour) + ":" + String(minute) + ":" + String(second);
  sendTelegramNotification(notification);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
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