#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define LED_PIN 2
#define BUZZER_PIN 32
#define MQ2_PIN 34    // Pin analog untuk sensor MQ-2
#define FLAME_PIN 35  // Pin digital untuk sensor api

// Konfigurasi WiFi
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// Initialize Telegram BOT
String BOTtoken = "1837465469:AAHGQzX5EzMhAGCKkHS8IiBvEJJ5t1e6O8c";  // your Bot Token (Get from Botfather)
String CHAT_ID = "108488036";

// Konfigurasi MQTT
const char* mqtt_server = "a02f84a8d83a48e7ae7b064d12537308.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;  // MQTT dengan SSL/TLS
const char* mqtt_user = "MIKRO";
const char* mqtt_password = "1DEAlist";

WiFiClientSecure espClient, clientTCP;
PubSubClient client(espClient);
UniversalTelegramBot bot(BOTtoken, clientTCP);

unsigned long lastSensor = 0;
unsigned long sensorTimeout = 1000;

unsigned long lastPublishTime = 0;
unsigned long publishTimeout = 10000;

int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

#define NUM_BUFFER_SIZE 10
char num_msg[NUM_BUFFER_SIZE];
int value = 0;

#define limitGas 1500
#define limitApi 1500

int adaGas = 0;
int adaApi = 0;
int gasValue = 0;
int apiValue = 0;

String pesan = "";
String lastPesan = "";

time_t tstamp;



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

time_t getTimestamp() {
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  return now;
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Pesan diterima dari topik: ");
  Serial.println(topic);

  Serial.print("Pesan: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  digitalWrite(LED_PIN, (char)payload[0] == '1' ? LOW : HIGH);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT... ");

    if (client.connect("ESP32_Client", mqtt_user, mqtt_password)) {
      Serial.println("Berhasil terhubung!");
      client.publish("testTopic", "ESP32 terhubung!");
      client.subscribe("testTopic");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  setup_wifi();

  Serial.print("Retrieving time: ");
  configTime(0, 0, "id.pool.ntp.org", "pool.ntp.org");  // get UTC time via NTP
  tstamp = getTimestamp();

  espClient.setInsecure();  // Gunakan koneksi tanpa sertifikat (opsional)

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  String ipNotif = "ESP32 Ready!";
  bot.sendMessage(CHAT_ID, ipNotif, "");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();


  if (millis() - lastSensor > sensorTimeout) {
    // Membaca status Flame Sensor
    apiValue = map(analogRead(FLAME_PIN), 0, 4095, 4095, 0);
    Serial.print("Intensitas Api: ");
    Serial.println(apiValue);

    // Membaca nilai MQ-2 (Gas LPG)
    gasValue = analogRead(MQ2_PIN);
    Serial.print("Intensitas Gas LPG: ");
    Serial.println(gasValue);

    lastSensor = millis();
  }


  if (millis() - lastPublishTime > publishTimeout) {
    // Mengirim data gas ke MQTT
    snprintf(num_msg, NUM_BUFFER_SIZE, "%d", gasValue);
    client.publish("sensor/gas_value", num_msg);

    snprintf(num_msg, NUM_BUFFER_SIZE, "%d", apiValue);
    client.publish("sensor/api_value", num_msg);

    Serial.println("Data sensor telah dikirim ke MQTT.");
    lastPublishTime = millis();
  }

  if (gasValue > limitGas) {
    adaGas = 1;
  } else {
    adaGas = 0;
  }

  if (apiValue > limitApi) {
    adaApi = 1;
  } else {
    adaApi = 0;
  }

  if (adaGas == 0 && adaApi == 0) {
    pesan = "Safe Condition";
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  } else if (adaGas == 1 && adaApi == 0) {
    pesan = "LPG Gas Leaked!";
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
  } else if (adaGas == 0 && adaApi == 1) {
    pesan = "Fire Detected!";
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
  } else if (adaGas == 1 && adaApi == 1) {
    pesan = "Emergency! This Building is on Fire!";
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
  }


  if (pesan != lastPesan) {

    Serial.println(pesan);
    sendTextMqtt("sensor/pesan", pesan);
    bot.sendMessage(CHAT_ID, pesan, "");
  }


  lastPesan = pesan;
}


void sendTextMqtt(char* topic, String text) {
  int textLength = text.length();

  // Tambahkan +1 untuk null-terminator
  char text_msg[textLength + 1];

  // Salin string dengan memastikan ada null-terminator
  text.toCharArray(text_msg, textLength + 1);

  Serial.print("TEXT : ");
  Serial.println(text_msg);

  client.publish(topic, text_msg);

  Serial.println("Data sensor telah dikirim ke MQTT.");
}
