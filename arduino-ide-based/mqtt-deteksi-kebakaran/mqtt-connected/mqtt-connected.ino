#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>

#define LED_PIN 2

// Konfigurasi WiFi
const char* ssid = "Warkop SELO";
const char* password = "kopisusu";

// Konfigurasi MQTT
const char* mqtt_server = "a02f84a8d83a48e7ae7b064d12537308.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;  // MQTT dengan SSL/TLS
const char* mqtt_user = "MIKRO";  // Ganti dengan username MQTT Anda
const char* mqtt_password = "1DEAlist";  // Ganti dengan password MQTT Anda

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

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
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println("\nWaktu telah disinkronkan.");
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
  pinMode(LED_PIN, OUTPUT);

  setup_wifi();
  setDateTime();

  espClient.setInsecure(); // Gunakan koneksi tanpa sertifikat (opsional)

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    snprintf(msg, MSG_BUFFER_SIZE, "hello world #%d", value++);
    Serial.print("Mengirim pesan: ");
    Serial.println(msg);
    client.publish("testTopic", msg);
  }
}
