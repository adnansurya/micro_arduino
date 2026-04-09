#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
// --- Konfigurasi WiFi ---
const char* ssid = "WARKOP LATEMMAMALA";
const char* password = "maret2026";
#define LED_BUILTIN 2 // On most ESP32 Dev boards

// --- Konfigurasi MQTT EMQX ---
const char* mqtt_server = "q31161f1.ala.asia-southeast1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_user = "MIKRO";
const char* mqtt_pass = "1DEAlist";
const char* topic_sub = "rc/control/Unit_01"; // Sesuaikan ID Unit

// --- Pin Hardware ---
const int RELAY_PIN = 26;  // Pin ke Relay
const int BUZZER_PIN = LED_BUILTIN; // Pin ke Buzzer/LED Indikator

// --- Variabel Global ---
WiFiClientSecure espClient;
PubSubClient client(espClient);
long remainingTime = 0; // Dalam detik
bool isActive = false;
unsigned long lastTick = 0;

void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

// Callback saat data masuk dari MQTT (GAS)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);

  // Logika menerima perintah dari GAS
  if (doc.containsKey("power")) {
    int pwr = doc["power"];
    if (pwr == 1) {
      remainingTime = doc["timer"]; // Ambil durasi dalam detik
      isActive = true;
      digitalWrite(RELAY_PIN, LOW); // Relay ON
      Serial.printf("Billing Start: %ld detik\n", remainingTime);
    } else {
      stopSystem();
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // ID Client unik untuk setiap ESP32
    if (client.connect("ESP32_RC_01", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe(topic_sub);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void stopSystem() {
  isActive = false;
  remainingTime = 0;
  digitalWrite(RELAY_PIN, HIGH); // Relay OFF
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("System Stopped/Finished");
}

void setup() {
  Serial.begin(115200);
  
  // Konfigurasi Pin
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // State Awal (Mati)
  digitalWrite(RELAY_PIN, HIGH); 
  digitalWrite(BUZZER_PIN, LOW);

  setup_wifi();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // --- Logika Timer Offline (Tanpa tergantung internet terus-menerus) ---
  unsigned long currentMillis = millis();
  if (isActive && currentMillis - lastTick >= 1000) {
    lastTick = currentMillis;
    remainingTime--;

    // Warning: Bunyi bip jika waktu < 60 detik
    if (remainingTime <= 60 && remainingTime > 0) {
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN)); // Toggle buzzer
    }

    // Cut-off jika waktu habis
    if (remainingTime <= 0) {
      stopSystem();
    }
    
    // Debug sisa waktu ke serial
    if (remainingTime % 10 == 0) { 
      Serial.printf("Sisa Waktu: %ld detik\n", remainingTime);
    }
  }
}