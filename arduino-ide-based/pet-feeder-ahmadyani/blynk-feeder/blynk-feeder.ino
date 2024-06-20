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

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "MIKRO";
char pass[] = "IDEAlist";

BlynkTimer mainTimer, scaleTimer;

HX711 scale;

#define loadCellDoutPin 26
#define loadCellSckPin 27

#define ledIndikatorPin 2
#define pirPin 13
#define servoPin 12
#define pompaPin 18
#define waterSensorPin 32

#define trigPin 15  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define echoPin 4   // Arduino pin tied to echo pin on the ultrasonic sensor.
#define maxDistance 200

Servo myservo = Servo();
NewPing sonar(trigPin, echoPin, maxDistance);

int modeOtomatis = 0;

String foodStat = "";
String waterStat = "";
int batasJarak = 15;

unsigned long foodDelay = 30000;
unsigned long foodLastOut = 0;

unsigned long waterDelay = 15000;
unsigned long waterLastOut = 0;
unsigned long waterDuration = 3000;
unsigned long waterOn = 0;
bool pompaOn = false;


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
  }
  digitalWrite(ledIndikatorPin, LOW);
}

BLYNK_WRITE(V4) {
  // Set incoming value from pin V0 to a variable
  digitalWrite(ledIndikatorPin, HIGH);
  int value = param.asInt();
  Serial.print("MINUM MANUAL: ");
  Serial.println(value);
  if (value == 1) {
    pompaOn = true;
    waterOn = millis();
  }
  digitalWrite(ledIndikatorPin, LOW);
}


// This function sends Arduino's uptime every second to Virtual Pin 2.
void mainEvent() {

  int foodObjek = digitalRead(pirPin);
  int waterAdc = analogRead(waterSensorPin);
  int jarakObjek = sonar.ping_cm();


  waterAdc = map(waterAdc, 0, 4096, 0, 10000);
  float waterPersen = (float)waterAdc / 100.0;
  Serial.print("PIR: ");
  Serial.print(foodObjek);
  Serial.print("\tPING: ");
  Serial.print(jarakObjek);
  Serial.print(" cm");
  Serial.print("\tWATER: ");
  Serial.print(waterPersen);
  Serial.println(" %");

  if (foodObjek) {
    digitalWrite(ledIndikatorPin, HIGH);
    foodStat = "Ada Objek";
    if (modeOtomatis) {
      if (millis() >= foodDelay + foodLastOut) {
        beriMakan();
        foodLastOut = millis();
      } else {
        Serial.println("Food Delay");
      }
    }
    digitalWrite(ledIndikatorPin, LOW);
  } else {
    foodStat = "Standby";
  }

  if (jarakObjek <= batasJarak) {
    waterStat = "Ada Objek";
    digitalWrite(ledIndikatorPin, HIGH);
    if (modeOtomatis) {
      if (pompaOn == false && millis() >= waterDelay + waterLastOut) {
        Serial.println("POMPA ON!");
        pompaOn = true;
        waterOn = millis();
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
    } else {
      digitalWrite(pompaPin, HIGH);
    }
  }

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

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  pinMode(pirPin, INPUT);

  initScale(241.265);

  myservo.write(servoPin, 90);
  digitalWrite(ledIndikatorPin, LOW);

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
