#include <ESP8266WiFi.h>
#include <NewPing.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define TRIGGER_PIN 12    // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN 14       // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200  // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

#define POMPA_PIN 13

#define WIFI_SSID "hwhw"
#define WIFI_PASSWORD "123456789"
//#define WIFI_SSID "Makassar Robotics"
//#define WIFI_PASSWORD "inovasiarduino"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "6124144417:AAE7g0q4YMcr9dlinMhBeJf7i01rbkFoC_c"

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 20, 4);


const long sensorToDasar = 24;
const long sensorToPenuh =2;
long penuh = sensorToDasar - sensorToPenuh;
float batasHabisPersen = 20;
String statusTangki = "NORMAL";

const int keringADC = 600;  // value for dry sensor
const int basahADC = 298;   // value for wet sensor
float batasKeringBasah = 20;

String teleMsg = "";
String lastMsg = "";
String idTujuan =   "308530485"; //"108488036"; 

String pesanKirim = "";
String statusPompa = "";
String lastPompa = "";
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
bool wifi_connect = false;

void setup() {
  // put your setup code here, to run once:'
  Serial.begin(9600);
  pinMode(POMPA_PIN, OUTPUT);
  digitalWrite(POMPA_PIN, LOW);

  // initialize the LCD
  lcd.begin();

  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.print("Initalizing...");

  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  lcd.setCursor(0,0);  
  lcd.print("Connecting to WiFi:");
  lcd.setCursor(0, 1);
  lcd.print(String(WIFI_SSID));
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setTrustAnchors(&cert);  // Add root certificate for api.telegram.org

  int wifi_count = 0;

  while ((WiFi.status() != WL_CONNECTED) && ((wifi_count / 2) < WAIT_SECOND)) {
    Serial.print(".");
    lcd.print(".");
    delay(500);
    wifi_count++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifi_connect = true;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  if (wifi_connect) {    
    Serial.print("\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
    lcd.print("Connnected to:");
    lcd.setCursor(0, 1);
    lcd.print(String(WIFI_SSID));  
    Serial.print("Retrieving time: ");
    configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
    time_t now = time(nullptr);
    while (now < 24 * 3600) {
      Serial.print(".");
      delay(100);
      now = time(nullptr);
    }
    Serial.println(now);
  } else {
    lcd.print("OFFLINE MODE");
    Serial.println("OFFLINE MODE");
  }
  delay(1500);
}

void loop() {

  if (wifi_connect && WiFi.status() != WL_CONNECTED){
    wifi_connect = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wifi Disconnected!");
    lcd.setCursor(0, 1);
    lcd.print("Switch to :");
    lcd.setCursor(0,2);
    lcd.print("Offline Mode");
    delay(2000);
  }
  // put your main code here, to run repeatedly:
  int sensorVal = analogRead(A0);
  int percentageHumidity = map(sensorVal, basahADC, keringADC, 100, 0);

  Serial.print("SENSOR_VAL: ");
  Serial.print(sensorVal);
  Serial.print("\tPERCENTAGE: ");
  Serial.print(percentageHumidity);
  Serial.println(" %");

 

  long jarak = sonar.ping_cm();
  Serial.print("Jarak: ");
  Serial.print(jarak);
  Serial.print(" cm");

  long tinggiAir = sensorToDasar - jarak;
  Serial.print("\tTinggi Air: ");
  Serial.print(tinggiAir);
  Serial.print(" cm");

  float persenTinggi = map(tinggiAir, penuh, 0, 100, 0);
   
  if (percentageHumidity < batasKeringBasah) {
    digitalWrite(POMPA_PIN, HIGH);
    statusPompa = "ON";
  } else {
    digitalWrite(POMPA_PIN, LOW);
    statusPompa = "OFF";
  }

  if(statusPompa != lastPompa){
    Serial.println("KIRIM NOTIF");
    String statPompa = "POMPA " + statusPompa;
          bot.sendMessage(idTujuan, statPompa, "");
          pesanKirim = "TANK=" + String(persenTinggi) + "% " + String(tinggiAir) + " cm\nTANK STATUS: " + statusTangki + "\nMOIST CONTENT= "+ String(percentageHumidity) + 
  "%\nW-PUMP STATUS:" + statusPompa;
  bot.sendMessage(idTujuan, pesanKirim, "");
  }
  
  
  if (persenTinggi <= batasHabisPersen) {
    statusTangki = "HABIS";
    teleMsg = "Isi Tangki Habis";

    digitalWrite(POMPA_PIN, LOW);
    if (lastMsg != teleMsg && wifi_connect) {
      bot.sendMessage(idTujuan, teleMsg, "");
      Serial.println("KIRIM NOTIF");
      
      pesanKirim = "TANK=" + String(persenTinggi) + "% " + String(tinggiAir) + " cm\nTANK STATUS: " + statusTangki + "\nMOIST CONTENT= "+ String(percentageHumidity) + 
  "%\nW-PUMP STATUS:" + statusPompa;
   bot.sendMessage(idTujuan, pesanKirim, "");
    
      lastMsg = teleMsg;
    }

  } else if (persenTinggi > batasHabisPersen && persenTinggi < 100) {
    statusTangki = "NORMAL";
  } else {
    statusTangki = "PENUH";
    teleMsg = "Tangki telah terisi penuh";
    if (lastMsg != teleMsg && wifi_connect) {
      bot.sendMessage(idTujuan, teleMsg, "");
      Serial.println("KIRIM NOTIF");
      
      pesanKirim = "TANK=" + String(persenTinggi) + "% " + String(tinggiAir) + " cm\nTANK STATUS: " + statusTangki + "\nMOIST CONTENT= "+ String(percentageHumidity) + 
  "%\nW-PUMP STATUS:" + statusPompa;
  bot.sendMessage(idTujuan, pesanKirim, "");
      lastMsg = teleMsg;
    }

    digitalWrite(POMPA_PIN, HIGH);
  }



  Serial.print("\tTANGKI: ");
  Serial.println(statusTangki);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TANK=");
  lcd.print(persenTinggi);
  lcd.print("% ");
  lcd.print(tinggiAir);
  lcd.print(" cm");
  lcd.setCursor(0, 1);
  lcd.print("TANK STATUS: ");
  lcd.print(statusTangki);
  lcd.setCursor(0, 2);
  lcd.print("MOIST CONTENT= ");
  lcd.print(percentageHumidity);
  lcd.print("%");
  lcd.setCursor(0, 3);
  lcd.print("W-PUMP STATUS: ");
  lcd.print(statusPompa);

  lastPompa = statusPompa;


  delay(1000);
}
