#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <WiFiClientSecure.h>

#define RELAY_SAKLAR 16  // Relay untuk saklar
#define RELAY_LAMPU 17   // Relay untuk lampu

#define offsetHour 8

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

String onTime, offTime;
int onMinute, onHour, offMinute, offHour;
int hour, minute, second;
bool jadwalOnState = false;
bool jadwalOffState = false;
bool lampuState = false;  // Variabel untuk menyimpan state lampu

time_t now;
unsigned long lastMillis = 0;
unsigned long refreshSecondMillis = 1;



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

void updateLampuState() {
  // Lampu menyala jika dalam rentang waktu onTime dan offTime
  now = time(nullptr);

  // Konversi ke struktur waktu lokal
  struct tm* timeInfo = localtime(&now);

  // Ambil jam dan menit
  hour = timeInfo->tm_hour;
  minute = timeInfo->tm_min;
  second = timeInfo->tm_sec;


  if (lampuState && jadwalOffState) {
    if (hour == offHour && minute == offMinute && second == 0) {
      Serial.println("OFF by Jadwal");
      lampuState = false;
    }
  }

  if (!lampuState && jadwalOnState) {
    if (hour == onHour && minute == onMinute && second == 0) {
      Serial.println("ON by Jadwal");
      lampuState = true;
    }
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
    digitalWrite(RELAY_SAKLAR, messageTemp == "saklar_on" ? LOW : HIGH);
  } else if (String(topic) == "lampu") {
    lampuState = (messageTemp == "lampu_on");
    updateLampuState();

  } else if (String(topic) == "lampu/jadwal/on") {
    onTime = messageTemp.c_str();
    onHour = onTime.substring(0, 2).toInt();
    onMinute = onTime.substring(3, 5).toInt();
    jadwalOnState = true;
    updateLampuState();

  } else if (String(topic) == "lampu/jadwal/off") {
    offTime = messageTemp.c_str();
    offHour = offTime.substring(0, 2).toInt();
    offMinute = offTime.substring(3, 5).toInt();
    jadwalOffState = true;
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
