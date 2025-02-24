#include <WiFi.h>
#include <HTTPClient.h>
#include <NewPing.h>

#define pirPin 35
#define trigPin 32
#define echoPin 33
#define buzzerPin 12

#define MAX_DISTANCE 200

NewPing sonar(trigPin, echoPin, MAX_DISTANCE);

// SSID dan password jaringan Wi-Fi
const char* ssid = "Wenzzz";
const char* password = "87654321";

String esp32CamIP = "http://192.168.12.79";

String ESP32IP;

int jarak = 0;
const int batasJarak = 180;

int deteksiPir = 0;
int deteksiPing = 0;
int lastPir = 0;
int lastPing = 0;

bool buzzerOn = false;

unsigned long buzzerTimerDetik = 20;
unsigned long lastBuzzerOn = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(pirPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Connect ke WiFi
  WiFi.begin(ssid, password);
  Serial.print(F("Menghubungkan ke WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  ESP32IP = WiFi.localIP().toString();
  Serial.println(F("\nWiFi Terhubung!"));
  Serial.print(F("IP Address ESP32: "));
  Serial.println(ESP32IP);

  blinkOut(buzzerPin, 2, 1000);
}

void loop() {
  // put your main code here, to run repeatedly:
  jarak = (int)sonar.ping_cm();
  delay(50);
  deteksiPir = digitalRead(pirPin);

  Serial.print(F("PIR: "));
  Serial.print(deteksiPir);
  Serial.print(F("\tJarak: "));
  Serial.println(jarak);

  if (jarak <= batasJarak && jarak > 0) {
    deteksiPing = 1;
  } else {
    deteksiPing = 0;
  }


  if (deteksiPir == 1 && lastPir == 0) {
    // blinkOut(buzzerPin, 2, 250);
    buzzerOn = true;
    openURL(esp32CamIP + "/on_pir");
    delay(3000);
  }

  if (deteksiPing == 1) {
    // blinkOut(buzzerPin, 2, 100);
    buzzerOn = true;
    openURL(esp32CamIP + "/on_ping");
    delay(3000);
  }

  if (buzzerOn) {
    digitalWrite(buzzerPin, HIGH);
    lastBuzzerOn = millis();
  } else {
    if (millis() > lastBuzzerOn + (buzzerTimerDetik * 1000)) {
      digitalWrite(buzzerPin, LOW);
    }
  }

  lastPir = deteksiPir;
  lastPing = deteksiPing;
  delay(100);
}


void openURL(String urlLink) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(urlLink);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Response: " + payload);
      // Lakukan sesuatu dengan data yang diterima (misalnya, kendalikan perangkat berdasarkan respons)
    } else {
      Serial.println(F("Error on HTTP request"));
      // blinkOut(buzzerPin, 3, 1500);
    }

    http.end();
    // delay(5000);  // Tunggu 5 detik sebelum mengirim permintaan berikutnya
  }
}

void blinkOut(int ledpin, int freq, int delayms) {
  for (int i = 0; i < freq; i++) {
    digitalWrite(ledpin, HIGH);
    delay(delayms);
    digitalWrite(ledpin, LOW);
    delay(delayms);
  }
}