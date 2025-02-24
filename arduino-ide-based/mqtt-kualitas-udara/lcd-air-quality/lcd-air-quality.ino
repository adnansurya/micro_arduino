#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <SPIFFS.h>

#define LED_PIN 2
#define MQ7_PIN 34     // Pin analog untuk MQ-7
#define MQ135_PIN 35   // Pin analog untuk MQ-135 (CO₂)
#define BUZZER_PIN 23  // Pin digital untuk buzzer

// Konfigurasi WiFi
// const char* ssid = "MIKRO";
// const char* password = "1DEAlist";
const char* ssid = "Cafe Tulus";
const char* password = "PesanDulu";

// Konfigurasi MQTT
const char* mqtt_server = "a02f84a8d83a48e7ae7b064d12537308.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;  // MQTT dengan SSL/TLS
const char* mqtt_user = "MIKRO";
const char* mqtt_password = "1DEAlist";

unsigned long lastLcd = 0;
unsigned long lcdTimeout = 1000;

unsigned long lastPublishTime = 0;
unsigned long publishTimeout = 10000;

WiFiClientSecure espClient;
PubSubClient client(espClient);

float mq7PPM, mq135PPM;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Ambang batas kualitas udara buruk dalam ppm
const float MQ7_THRESHOLD_PPM = 50.0;     // CO berbahaya
const float MQ135_THRESHOLD_PPM = 800.0;  // CO₂ tidak sehat (> 800 ppm)

// Nilai Ro sensor (harus dikalibrasi)
float Ro_MQ7 = 10.0;
float Ro_MQ135 = 9.0;

String pesan = "";
String lastPesan = "";

time_t tstamp;

#define NUM_BUFFER_SIZE 10
char num_msg[NUM_BUFFER_SIZE];
int value = 0;

unsigned long standbyTimer = 120000;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  Serial.println("Menghubungkan ke WiFi...");
  lcd.setCursor(0, 0);
  lcd.print("Connecting to:");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Terhubung ke");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  Serial.println("\nWiFi terhubung");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "id.pool.ntp.org", "pool.ntp.org");  // get UTC time via NTP
  tstamp = getTimestamp();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);  

  while (millis() <= standbyTimer) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Preparing...");
    lcd.setCursor(0, 1);

    lcd.print("(");
    lcd.print((standbyTimer - millis()) / 1000);
    lcd.print(" sec)");
    delay(1000);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready");
  delay(2000);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();



  Serial.print("CO (MQ-7): ");
  Serial.print(mq7PPM);
  Serial.print(" ppm | ");
  Serial.print("CO2 (MQ-135): ");
  Serial.print(mq135PPM);
  Serial.println(" ppm");


  // Nyalakan buzzer jika kualitas udara buruk
  if (mq7PPM > MQ7_THRESHOLD_PPM) {
    pesan = "Kadar CO Melebihi Batas!";
    digitalWrite(BUZZER_PIN, HIGH);

  } else if (mq135PPM > MQ135_THRESHOLD_PPM) {
    pesan = "Kadar CO2 Melebihi Batas!";
    digitalWrite(BUZZER_PIN, HIGH);

  } else if (mq7PPM > MQ7_THRESHOLD_PPM && mq135PPM > MQ135_THRESHOLD_PPM) {
    pesan = "Kualitas Udara Buruk!";
    digitalWrite(BUZZER_PIN, HIGH);

  } else {
    pesan = "Kualitas Udara Normal";
    digitalWrite(BUZZER_PIN, LOW);
  }


  if (millis() - lastLcd > lcdTimeout) {

    lcdUpdate();

    lastLcd = millis();
  }


  if (millis() - lastPublishTime > publishTimeout) {
    // Mengirim data gas ke MQTT
    snprintf(num_msg, NUM_BUFFER_SIZE, "%f", mq7PPM);
    client.publish("sensor/co_value", num_msg);

    snprintf(num_msg, NUM_BUFFER_SIZE, "%f", mq135PPM);
    client.publish("sensor/co2_value", num_msg);

    Serial.println("Data sensor telah dikirim ke MQTT.");
    lastPublishTime = millis();
  }

  if (pesan != lastPesan) {
    Serial.println(pesan);
    sendTextMqtt("sensor/pesan", pesan);
  }


  lastPesan = pesan;

  delay(500);
}

void lcdUpdate() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO: ");
  lcd.print(mq7PPM);
  lcd.print(" ppm");

  lcd.setCursor(0, 1);
  lcd.print("CO2: ");
  lcd.print(mq135PPM);
  lcd.print(" ppm");
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


void getSensorValue() {
  int mq7Raw = analogRead(MQ7_PIN);
  int mq135Raw = analogRead(MQ135_PIN);


  // Hitung Rs untuk masing-masing sensor
  float mq7Rs = (4095.0 / mq7Raw - 1.0) * Ro_MQ7;
  float mq135Rs = (4095.0 / mq135Raw - 1.0) * Ro_MQ135;

  // Konversi ke PPM
  mq7PPM = 1000 * pow(mq7Rs / Ro_MQ7, -1.5);             // MQ-7 (CO)
  // mq7PPM = 10 * pow(mq7Rs / Ro_MQ7, -1.5);
  mq135PPM = 116.602 * pow(mq135Rs / Ro_MQ135, -2.769);  // MQ-135 (CO₂)
}
