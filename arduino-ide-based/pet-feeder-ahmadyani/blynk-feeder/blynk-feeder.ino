#define BLYNK_TEMPLATE_ID "TMPL6AAYXbGy1"
#define BLYNK_TEMPLATE_NAME "Quickstart Device"
#define BLYNK_AUTH_TOKEN "xsH7Nl_fpuErLvySxjeASiPa5fmjzvAp"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <NewPing.h>
#include <Servo.h>
#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "MIKRO";
char pass[] = "IDEAlist";

BlynkTimer mainTimer, scaleTimer;

HX711 scale;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

#define loadCellDoutPin 26
#define loadCellSckPin 27

#define ledIndikatorPin 2
#define servoPin 14
#define pompaPin 4
#define waterSensorPin 32


// #define trigPin 16  // Arduino pin tied to trigger pin on the ultrasonic sensor.
// #define echoPin 4   // Arduino pin tied to echo pin on the ultrasonic sensor.
#define pirPin 16
#define maxDistance 200

#define GMT_OFFSET 8

time_t epochTime;
String weekDay, formattedTime, currentDate, waktu, tStamp;
int currentYear = 0;

int jadwalMakan[] = { 9, 19, 29, 39, 49, 59 };
int lastHour, lastMinute, lastSecond;

Servo myservo = Servo();
// NewPing sonar(trigPin, echoPin, maxDistance);

int modeOtomatis = 0;

String foodStat = "";
String waterStat = "";
int batasJarak = 15;

unsigned long foodDelay = 30000;
unsigned long foodLastOut = 0;

unsigned long waterDelay = 15000;
unsigned long waterLastOut = 0;
unsigned long waterDuration = 2000;
unsigned long waterOn = 0;
bool pompaOn = false;

String camIPEnd = "1";
String camIP = "";
String camURL = "";

rtc_cpu_freq_config_t config;
// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0) {
  // Set incoming value from pin V0 to a variable
  digitalWrite(ledIndikatorPin, HIGH);
  int value = param.asInt();
  modeOtomatis = value;
  Serial.print("MODE AUTO: ");
  Serial.println(modeOtomatis);
  digitalWrite(ledIndikatorPin, LOW);
}

BLYNK_WRITE(V1) {
  // Set incoming value from pin V0 to a variable
  digitalWrite(ledIndikatorPin, HIGH);
  int value = param.asInt();
  Serial.print("MAKAN MANUAL: ");
  Serial.println(value);



  if (value == 1) {

    beriMakan();
    camURL = camIP + "/makan_manual";
    openURL(camURL);
  }
  digitalWrite(ledIndikatorPin, LOW);
}

BLYNK_WRITE(V4) {
  // Set incoming value from pin V0 to a variable

  int value = param.asInt();

  if (value == 1) {
    digitalWrite(ledIndikatorPin, HIGH);
    digitalWrite(pompaPin, HIGH);
    delay(waterDuration);
    Serial.println("POMPA ON MANUAL!");
    digitalWrite(pompaPin, LOW);

    camURL = camIP + "/minum_manual";
    openURL(camURL);
    digitalWrite(ledIndikatorPin, LOW);
  }
}


// This function sends Arduino's uptime every second to Virtual Pin 2.
void mainEvent() {

  timeClient.update();
  formattedTime = timeClient.getFormattedTime();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();
  Serial.print("WAKTU : ");
  Serial.print(formattedTime);

  int waterAdc = analogRead(waterSensorPin);
  // int jarakObjek = sonar.ping_cm();
  int waterObjek = digitalRead(pirPin);

  waterAdc = map(waterAdc, 0, 4096, 0, 10000);
  float waterPersen = (float)waterAdc / 100.0;
  Serial.print("\tPIR: ");
  Serial.print(waterObjek);  
  Serial.print("\tWATER: ");
  Serial.print(waterPersen);
  Serial.println(" %");

  if (modeOtomatis) {
    if (saatnyaMakan(currentMinute) == true && belumMakan(currentMinute, lastMinute) == true) {
      digitalWrite(ledIndikatorPin, HIGH);
      beriMakan();
      camURL = camIP + "/makan_auto";
      openURL(camURL);
      digitalWrite(ledIndikatorPin, LOW);
    }
  }

  if (waterObjek == true) {
    waterStat = "Ada Objek";
    digitalWrite(ledIndikatorPin, HIGH);
    if (modeOtomatis) {
      if (pompaOn == false && millis() >= waterDelay + waterLastOut) {
        Serial.println("POMPA ON!");
        digitalWrite(pompaPin, HIGH);
        pompaOn = true;
        waterOn = millis();
        camURL = camIP + "/minum_auto";
        openURL(camURL);
      } else {
        Serial.println("Water Delay");
      }
    } else {
      pompaOn = false;
    }
    digitalWrite(ledIndikatorPin, LOW);
  } else {
    waterStat = "Standby";
  }



  if (pompaOn) {

    if (millis() >= (waterOn + waterDuration)) {
      Serial.println("POMPA OFF!");
      pompaOn = false;
      digitalWrite(pompaPin, LOW);
      waterLastOut = millis();
    }
  }

  lastHour = currentHour;
  lastMinute = currentMinute;
  lastSecond = currentSecond;

  Blynk.virtualWrite(V3, waterPersen);
}

void getBerat() {


  scale.power_up();
  float berat = scale.get_units();
  Serial.print(berat);
  Serial.println(" gram");
  Blynk.virtualWrite(V2, berat);
  scale.power_down();
}

void setup() {
  // Debug console
  Serial.begin(115200);

  pinMode(ledIndikatorPin, OUTPUT);
  digitalWrite(ledIndikatorPin, HIGH);

  pinMode(pompaPin, OUTPUT);
  digitalWrite(pompaPin, LOW);

  Blynk.connectWiFi(ssid, pass);

  Blynk.config(BLYNK_AUTH_TOKEN);


  initScale(241.265);

  myservo.write(servoPin, 90);
  digitalWrite(ledIndikatorPin, LOW);

  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
  camIP = "http://" + String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + camIPEnd;

  timeClient.begin();
  timeClient.setTimeOffset(GMT_OFFSET * 3600);

  // Setup a function to be called every second
  mainTimer.setInterval(1000L, mainEvent);
  scaleTimer.setInterval(5000L, getBerat);
}

void loop() {
  Blynk.run();
  mainTimer.run();
  scaleTimer.run();
  // You can inject your own code or combine it with other sketches.
  // Check other examples on how to communicate with Blynk. Remember
  // to avoid delay() function!
}

void initScale(float cal_factor) {

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

  scale.set_scale(cal_factor);
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
  Serial.println(scale.get_units(5), 1);
}

void beriMakan() {
  Serial.println("BERI MAKAN!");


  for (int u = 0; u < 10; u++) {
    for (int pos = 0; pos <= 180; pos++) {  // go from 0-180 degrees
      myservo.write(servoPin, pos);         // set the servo position (degrees)
      delay(1);
    }
    for (int pos = 180; pos >= 0; pos--) {  // go from 180-0 degrees
      myservo.write(servoPin, pos);         // set the servo position (degrees)
      delay(1);
    }
  }
  myservo.write(servoPin, 90);
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

bool saatnyaMakan(int waktuSekarang) {
  bool waktuMakan = false;
  int banyakJadwal = sizeof(jadwalMakan) / sizeof(int);
  for (int j = 0; j < banyakJadwal; j++) {
    if (jadwalMakan[j] == waktuSekarang) {
      waktuMakan = true;
      break;
    }
  }
  return waktuMakan;
}

bool belumMakan(int waktuSekarang, int waktuSebelumnya) {
  Serial.print("NOW : ");
  Serial.print(waktuSekarang);
  Serial.print("\tLAST : ");
  Serial.println(waktuSebelumnya);
  if (waktuSekarang != waktuSebelumnya) {
    return true;
  } else {
    return false;
  }
}
