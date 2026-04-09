#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// --- Config ---
const char* ssid = "WARKOP LATEMMAMALA";
const char* password = "maret2026";
const char* mqtt_server = "q31161f1.ala.asia-southeast1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_user = "MIKRO";
const char* mqtt_pass = "1DEAlist";

const char* topic_sub = "rc/control/Unit_1";  // Topik Terima Perintah
const char* topic_pub = "rc/update/Unit_1";   // Topik Kirim Update Sisa Waktu
const char* UNIT_ID = "1";

const int RELAY_PIN = 26; 
const int BUZZER_PIN = 27;

WiFiClientSecure espClient;
PubSubClient client(espClient);

long remainingTime = 0;
bool isActive = false;
unsigned long lastTick = 0;

void setup_wifi() {
  Serial.print("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);

  if (doc.containsKey("power") && doc["power"] == 1) {
    remainingTime = doc["timer"];
    isActive = true;
    digitalWrite(RELAY_PIN, LOW); // ON
    Serial.printf("START: %ld detik\n", remainingTime);
    sendUpdate(remainingTime); // Update awal ke GAS
  }
}

void sendUpdate(long sisa) {
  StaticJsonDocument<100> doc;
  doc["unitID"] = UNIT_ID;
  doc["sisa"] = sisa;
  char buffer[100];
  serializeJson(doc, buffer);
  client.publish(topic_pub, buffer);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT Connecting...");
    if (client.connect("ESP32_RC_UNIT1", mqtt_user, mqtt_pass)) {
      Serial.println("Connected");
      client.subscribe(topic_sub);
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Awal OFF

  setup_wifi();
  espClient.setInsecure(); // Bypass SSL Certificate
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long currentMillis = millis();
  if (isActive && currentMillis - lastTick >= 1000) {
    lastTick = currentMillis;
    remainingTime--;

    // Update Spreadsheet tiap 60 detik, saat 30 detik, dan saat 0
    if (remainingTime % 60 == 0 || remainingTime == 30 || remainingTime == 0) {
      sendUpdate(remainingTime);
    }

    // Buzzer Warning < 60 detik
    if (remainingTime <= 60 && remainingTime > 0) {
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    }

    // Cut-off
    if (remainingTime <= 0) {
      isActive = false;
      digitalWrite(RELAY_PIN, HIGH); // OFF
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("Time Up!");
    }
  }
}