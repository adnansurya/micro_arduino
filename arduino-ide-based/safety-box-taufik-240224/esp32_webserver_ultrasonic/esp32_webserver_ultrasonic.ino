#include <NewPing.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <HTTPClient.h>

#include <WebServer.h>
#include <ESPmDNS.h>

WebServer server(80);

#define BOT_TOKEN "1115765927:AAFgDI003Xn41tererJRuoU543tBsg8CBpE"
#define CHAT_ID "108488036"

#define TRIGGER_PIN 19    // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN 18       // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200  // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

#define SOLE1_PIN 5
#define SOLE2_PIN 4
#define SOLE3_PIN 17
#define SOLE4_PIN 16

#define ledPin 2


char ssid[] = "MIKRO";
char pass[] = "IDEAlist";

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);  // NewPing setup of pins and maximum distance.
LiquidCrystal_I2C lcd(0x27, 20, 4);

int sensorCheckDelay = 100;
unsigned long lastTimeCheckSensor;

int lcdCheckDelay = 1000;
unsigned long lastTimeCheckLcd;

int adaGerak = 0;
int lastGerak = 0;

bool open1 = false;
bool open2 = false;
bool open3 = false;

long soleOpenTimeOut = 20000;

unsigned long sole1OpenTime = 0;
unsigned long sole2OpenTime = 0;
unsigned long sole3OpenTime = 0;

String camIP = "http://192.168.101.133";
String camURL = camIP + "/ada_gerakan";

int jarak;
const int batasJarak = 30;


void setup() {
  lcd.init();
  // lcd.begin();
  lcd.backlight();

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);
  Serial.print("Connecting to WiFi");
  lcdPrompt("Connecting to Wifi ", 0);

  pinMode(SOLE1_PIN, OUTPUT);
  pinMode(SOLE2_PIN, OUTPUT);
  pinMode(SOLE3_PIN, OUTPUT);
  pinMode(SOLE4_PIN, OUTPUT);

  digitalWrite(SOLE1_PIN, HIGH);
  digitalWrite(SOLE2_PIN, HIGH);
  digitalWrite(SOLE3_PIN, HIGH);
  digitalWrite(SOLE4_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    lcd.print(".");
    kedipLed(0.2);
  }

  lcdPrompt("Connected to :" + String(ssid), 2000);
  // Print ESP32 Local IP Address
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
  camIP = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + ".133";
  camURL = "http://" + camIP + "/ada_gerakan";





  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot);
  server.on("/on1", on1);
  server.on("/off1", off1);
  server.on("/on2", on2);
  server.on("/off2", off2);
  server.on("/on3", on3);
  server.on("/off3", off3);
  server.begin();
  lcdPrintAll("IP Address : ", ip.toString(), "CAM IP:", camIP, 5000);
  kedipLed(2);
  lcdStandby();
}

void loop() {



  if (millis() > lastTimeCheckSensor + sensorCheckDelay) {

    jarak = sonar.ping_cm();  // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
    Serial.print("Ping: ");
    Serial.print(jarak);  // Send ping, get distance in cm and print result (0 = outside set distance range)
    Serial.println("cm");

    if (jarak <= batasJarak && jarak != 0) {
      adaGerak = 1;
    } else {
      adaGerak = 0;
    }
    if (adaGerak != lastGerak) {  //filter untuk mendeteksi perubahan status
      if (adaGerak) {
        kedipLed(0.2);
        Serial.println("Gerakan Terdeteksi");
        openURL(camURL);
        lcdPrintAll("Gerakan Terdeteksi", "Mengirim Foto ", "      ke Telegram", "", 3000);
      }
    }
    lastGerak = adaGerak;
    lastTimeCheckSensor = millis();
  } else {

    if (millis() > lastTimeCheckLcd + lcdCheckDelay) {
      lcdStandby();
      lastTimeCheckLcd = millis();
    }

    if (open1) {
      if (millis() > sole1OpenTime + soleOpenTimeOut) {
        off1();
      }
    }

    if (open2) {
      if (millis() > sole2OpenTime + soleOpenTimeOut) {
        off2();
      }
    }

    if (open3) {
      if (millis() > sole3OpenTime + soleOpenTimeOut) {
        off3();
      }
    }

    server.handleClient();
  }
}

void handleRoot() {
  server.send(200, "text/plain", "hello from esp32!");
}

void on1() {
  kedipLed(0.2);
  server.send(200, "text/plain", "Membuka Kunci 1...");
  digitalWrite(SOLE1_PIN, LOW);
  open1 = true;
  sole1OpenTime = millis();
  lcdPrompt("Membuka Kunci 1... ", 2000);
}

void off1() {
  kedipLed(0.2);
  server.send(200, "text/plain", "Mengunci Laci 1...");
  digitalWrite(SOLE1_PIN, HIGH);
  open1 = false;
  lcdPrompt("Mengunci Laci 1... ", 2000);
}

void on2() {
  kedipLed(0.2);
  server.send(200, "text/plain", "Membuka Kunci 2...");
  digitalWrite(SOLE2_PIN, LOW);
  open2 = true;
  sole2OpenTime = millis();
  lcdPrompt("Membuka Kunci 2... ", 2000);
}

void off2() {
  kedipLed(0.2);
  server.send(200, "text/plain", "Mengunci Laci 2...");
  digitalWrite(SOLE2_PIN, HIGH);
  open2 = false;
  lcdPrompt("Mengunci Laci 2... ", 2000);
}

void on3() {
  kedipLed(0.2);
  server.send(200, "text/plain", "Membuka Kunci 3...");
  digitalWrite(SOLE3_PIN, LOW);
  open3 = true;
  sole3OpenTime = millis();
  lcdPrompt("Membuka Kunci 3... ", 2000);
}

void off3() {
  kedipLed(0.2);
  server.send(200, "text/plain", "Mengunci Laci 3...");
  digitalWrite(SOLE3_PIN, HIGH);
  open3 = false;
  lcdPrompt("Mengunci Laci 3... ", 2000);
}

void lcdPrintAll(String baris1, String baris2, String baris3, String baris4, int jeda) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2);
  lcd.setCursor(0, 2);
  lcd.print(baris3);
  lcd.setCursor(0, 3);
  lcd.print(baris4);
  delay(jeda);
}

void lcdPrompt(String prompt, int jeda) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  delay(jeda);
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
      Serial.println("Error on HTTP request");
    }

    http.end();
    // delay(5000);  // Tunggu 5 detik sebelum mengirim permintaan berikutnya
  }
}

void lcdStandby() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Status -- > STANDBY");

  lcd.setCursor(0, 1);
  if (open1) {
    lcd.print("Laci 1: Terbuka");
  } else {
    lcd.print("Laci 1: Terkunci");
  }

  lcd.setCursor(0, 2);
  if (open2) {
    lcd.print("Laci 2: Terbuka");
  } else {
    lcd.print("Laci 2: Terkunci");
  }

  lcd.setCursor(0, 3);
  if (open3) {
    lcd.print("Laci 3: Terbuka");
  } else {
    lcd.print("Laci 3: Terkunci");
  }
}

void kedipLed(float detik) {
  digitalWrite(ledPin, HIGH);
  delay(detik * 1000);
  digitalWrite(ledPin, LOW);
}
