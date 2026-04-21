#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h> // Library WiFiManager ditambahkan

// --- Konfigurasi WiFi ---
// ssid dan password dihapus karena sudah dihandle WiFiManager

// --- Konfigurasi MQTT EMQX ---
const char* mqtt_server = "q31161f1.ala.asia-southeast1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_user = "MIKRO";
const char* mqtt_pass = "1DEAlist";

// --- Identitas Unit ---
const char* UNIT_ID = "1";
const char* topic_sub = "rc/control/Unit_1";
const char* topic_pub = "rc/update/Unit_1";

// --- Pin Hardware ---
const int RELAY_PIN = 26;
const int LED_PIN = 25;
const int BUZZER_PIN = 27;

// --- Variabel Global ---
WiFiClientSecure espClient;
PubSubClient client(espClient);

long remainingTime = 0;
bool isActive = false;
bool isPaused = false;
unsigned long lastTick = 0;

// Fungsi mengirim data ke Google Sheets via MQTT Webhook
void sendUpdateToGAS(long sisa) {
  StaticJsonDocument<128> doc;
  doc["unitID"] = UNIT_ID;
  doc["sisa"] = sisa;

  char buffer[128];
  serializeJson(doc, buffer);
  client.publish(topic_pub, buffer);
  Serial.printf("Sent update to GAS: %ld s\n", sisa);
}

void stopSystem() {
  isActive = false;
  isPaused = false;
  remainingTime = 0;
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  sendUpdateToGAS(0);
  Serial.println("System Stopped.");
}

void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
  String action = doc["action"];

  if (action == "start") {
    remainingTime = doc["timer"];
    isActive = true;
    isPaused = false;
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, HIGH);
    sendUpdateToGAS(remainingTime);
  } else if (action == "pause") {
    isPaused = true;
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
  } else if (action == "resume") {
    isPaused = false;
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, HIGH);
  } else if (action == "stop") {
    stopSystem();
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32_RC_UNIT_";
    clientId += UNIT_ID;

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe(topic_sub);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // --- Integrasi WiFiManager ---
  WiFiManager wm;

  // Hapus baris di bawah ini jika tidak ingin reset settingan WiFi setiap kali nyala
  // wm.resetSettings(); 

  // Menampilkan Portal Konfigurasi jika WiFi tidak ditemukan
  // Nama AP yang muncul: "ESP32_Config_Unit_1"
  bool res;
  res = wm.autoConnect("ESP32_Config_Unit_1"); 

  if(!res) {
      Serial.println("Gagal terhubung ke WiFi");
      // ESP.restart();
  } else {
      Serial.println("WiFi Connected via WiFiManager!");
  }

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  if (isActive && !isPaused && (currentMillis - lastTick >= 1000)) {
    lastTick = currentMillis;
    remainingTime--;

    if (remainingTime % 60 == 0 || remainingTime == 30 || remainingTime == 0) {
      sendUpdateToGAS(remainingTime);
    }

    if (remainingTime <= 60 && remainingTime > 0) {
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    }

    if (remainingTime <= 0) {
      stopSystem();
    }
  }
}