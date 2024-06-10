
#include <Servo.h>
#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Wifi network station credentials
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "IDEAlist"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "1389983359:AAHGPAMEwdwmO6Bd_PgtU1cvFPNC65r-oXM"

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done


HX711 scale;

#define loadCellDoutPin 16
#define loadCellSckPin 4

#define ledIndikatorPin 2
#define pirPin 19


Servo myservo = Servo();

const int servoPin = 23;

int adaGerak = 0;
int lastGerak = 0;

void setup() {
  Serial.begin(115200);



  pinMode(ledIndikatorPin, OUTPUT);
  digitalWrite(ledIndikatorPin, HIGH);

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  pinMode(pirPin, INPUT);



  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  rtc_clk_cpu_freq_to_config(RTC_CPU_FREQ_80M, &config);
  rtc_clk_cpu_freq_set_config_fast(&config);
  Serial.println("HX711 Demo");

  Serial.println("Initializing the scale");

  scale.begin(loadCellDoutPin, loadCellSckPin);

  Serial.println("Before setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read());  // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));  // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));  // print the average of 5 readings from the ADC minus the tare weight (not set yet)

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);  // print the average of 5 readings from the ADC minus tare weight (not set) divided
                                          // by the SCALE parameter (not set yet)

  scale.set_scale(73.89);
  //scale.set_scale(-471.497);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();  // reset the scale to 0

  Serial.println("After setting up the scale:");

  Serial.print("read: \t\t");
  Serial.println(scale.read());  // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));  // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));  // print the average of 5 readings from the ADC minus the tare weight, set with tare()

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);  // print the average of 5 readings from the ADC minus tare weight, divided
                                          // by the SCALE parameter set with set_scale

  Serial.println("Readings:");
  myservo.write(servoPin, 90);
  delay(2000);
  digitalWrite(ledIndikatorPin, LOW);
}

void loop() {

  adaGerak = digitalRead(pirPin);
  Serial.print("SENSOR : ");
  Serial.println(adaGerak);

  if (lastGerak != adaGerak) {
    if (adaGerak == 1) {
      Serial.println("GERAKAN TERDETEKSI !");
      digitalWrite(ledIndikatorPin, HIGH);
       bot.sendMessage("108488036", "ADA KUCING!", "");
      for (int u = 0; u < 10; u++) {
        beriMakan();
      }
      myservo.write(servoPin, 90);
      delay(2000);
      Serial.print("Berat:\t");
      // Serial.print(scale.get_units(), 1);
      float berat = scale.get_units();
      Serial.print(berat);
      Serial.println(" gram");


      scale.power_down();  // put the ADC in sleep mode
      String pesan = "SISA MAKANAN : " + String(berat) + " gram";
      bot.sendMessage("108488036", pesan, "");
      delay(5000);
      scale.power_up();

      digitalWrite(ledIndikatorPin, LOW);

    } else {
      delay(500);
    }
  }
  lastGerak = adaGerak;
  delay(200);

  // beriMakan();
}

void beriMakan() {

  for (int pos = 0; pos <= 180; pos++) {  // go from 0-180 degrees
    myservo.write(servoPin, pos);         // set the servo position (degrees)
    delay(1);
  }
  for (int pos = 180; pos >= 0; pos--) {  // go from 180-0 degrees
    myservo.write(servoPin, pos);         // set the servo position (degrees)
    delay(1);
  }
}
