#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <WiFiClientSecure.h>

#define RELAY_SAKLAR 16  // Relay untuk saklar
#define RELAY_LAMPU 17   // Relay untuk lampu

// Konfigurasi WiFi
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// Konfigurasi MQTT
const char* mqtt_server = "a02f84a8d83a48e7ae7b064d12537308.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "MIKRO";
const char* mqtt_password = "1DEAlist";

WiFiClientSecure espClient;
PubSubClient client(espClient);

time_t onTime = 0;
time_t offTime = 0;
bool lampuState = false; // Variabel untuk menyimpan state lampu

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

void updateLampuState() {
  // Lampu menyala jika dalam rentang waktu onTime dan offTime
  time_t now = time(nullptr);
  if (lampuState || (now >= onTime && now < offTime)) {
    digitalWrite(RELAY_LAMPU, LOW); // Menyalakan relay
  } else {
    digitalWrite(RELAY_LAMPU, HIGH); // Mematikan relay
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
    digitalWrite(RELAY_SAKLAR, messageTemp == "saklar_on" ? LOW : HIGH);
  } else if (String(topic) == "lampu") {
    lampuState = (messageTemp == "lampu_on");
    updateLampuState();
  } else if (String(topic) == "lampu/jadwal/on") {
    onTime = atol(messageTemp.c_str());
    updateLampuState();
  } else if (String(topic) == "lampu/jadwal/off") {
    offTime = atol(messageTemp.c_str());
    updateLampuState();
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
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  updateLampuState();
}
