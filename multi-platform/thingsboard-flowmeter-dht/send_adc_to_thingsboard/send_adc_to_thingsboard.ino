#include <WiFi.h>
#include <PubSubClient.h>

// Set WiFi credentials
const char* ssid = "MIKRO";
const char* password = "IDEAlist";

// Set ThingsBoard credentials
const char* mqtt_server = "52.185.184.82"; // atau server ThingsBoard anda
const char* token = "i8rju4gslhc29sc7s3wv"; // Token perangkat dari ThingsBoard

WiFiClient espClient;
PubSubClient client(espClient);

// Set ADC pin
const int adcPin = 34; // Anda bisa menggunakan pin ADC lain sesuai kebutuhan

long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  pinMode(adcPin, INPUT);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Client", token, NULL)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    int adcValue = analogRead(adcPin);
    
    String payload = "{";
    payload += "\"adc_value\":"; payload += adcValue;
    payload += "}";

    payload.toCharArray(msg, 50);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("v1/devices/me/telemetry", msg);
  }
}
