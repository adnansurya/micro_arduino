#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// --- Konfigurasi WiFi ---
const char* ssid = "WARKOP LATEMMAMALA";
const char* password = "maret2026";

// --- Konfigurasi MQTT EMQX ---
const char* mqtt_server = "q31161f1.ala.asia-southeast1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_user = "MIKRO";
const char* mqtt_pass = "1DEAlist";

// --- Identitas Unit ---
const char* UNIT_ID = "1";
const char* topic_sub = "rc/control/Unit_1";  // Mendengarkan perintah (Start/Pause/Stop)
const char* topic_pub = "rc/update/Unit_1";   // Mengirim status/sisa waktu ke GAS

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

void setup_wifi() {
  delay(10);
  Serial.print("\nConnecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

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
  digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);  // Buzzer OFF
  sendUpdateToGAS(0);             // Kirim status 0 (Selesai)
  Serial.println("System Stopped.");
}

// Callback menerima pesan dari Telegram -> GAS -> EMQX
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);

  String action = doc["action"];

  if (action == "start") {
    remainingTime = doc["timer"];  // durasi dalam detik
    isActive = true;
    isPaused = false;
    digitalWrite(RELAY_PIN, LOW);  // Relay ON
    digitalWrite(LED_PIN, HIGH);
    sendUpdateToGAS(remainingTime);
    Serial.println("Action: START");
  } else if (action == "pause") {
    isPaused = true;
    digitalWrite(RELAY_PIN, HIGH);  // Relay OFF
    digitalWrite(LED_PIN, LOW);
    Serial.println("Action: PAUSE");
  } else if (action == "resume") {
    isPaused = false;
    digitalWrite(RELAY_PIN, LOW);  // Relay ON
    digitalWrite(LED_PIN, HIGH);
    Serial.println("Action: RESUME");
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
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Posisi awal: Relay Mati (Active Low)
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  setup_wifi();

  // Konfigurasi SSL (Insecure karena menggunakan EMQX Cloud/Self-Signed)
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

  // Logika Timer: Berjalan hanya jika ACTIVE dan TIDAK PAUSE
  if (isActive && !isPaused && (currentMillis - lastTick >= 1000)) {
    lastTick = currentMillis;
    remainingTime--;

    // 1. Update berkala ke Spreadsheet tiap 60 detik, atau saat 30 detik terakhir
    if (remainingTime % 60 == 0 || remainingTime == 30 || remainingTime == 0) {
      sendUpdateToGAS(remainingTime);
    }

    // 2. Warning Buzzer (Bunyi putus-putus jika waktu < 1 menit)
    if (remainingTime <= 60 && remainingTime > 0) {
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    }

    // 3. Auto Cut-Off jika waktu habis
    if (remainingTime <= 0) {
      stopSystem();
      Serial.println("Time's Up! Relay cut-off.");
    }
  }
}