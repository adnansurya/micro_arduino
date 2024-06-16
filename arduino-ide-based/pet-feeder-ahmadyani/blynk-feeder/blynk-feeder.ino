#define BLYNK_TEMPLATE_ID "TMPL6AAYXbGy1"
#define BLYNK_TEMPLATE_NAME "Quickstart Device"
#define BLYNK_AUTH_TOKEN "xsH7Nl_fpuErLvySxjeASiPa5fmjzvAp"

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

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

#define loadCellDoutPin 16
#define loadCellSckPin 4

#define ledIndikatorPin 2
#define pirPin 19
#define servoPin 23

Servo myservo = Servo();

int adaGerak = 0;
int lastGerak = 0;
rtc_cpu_freq_config_t config;
// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0) {
  // Set incoming value from pin V0 to a variable
  int value = param.asInt();

  // Update state
  Blynk.virtualWrite(V1, value);
}


// This function sends Arduino's uptime every second to Virtual Pin 2.
void mainEvent() {

  Blynk.virtualWrite(V0, millis() / 1000);
  adaGerak = digitalRead(pirPin);
  Serial.print("SENSOR : ");
  Serial.println(adaGerak);
  Blynk.virtualWrite(V1, adaGerak);
  lastGerak = adaGerak;
}

void getBerat() {
  digitalWrite(ledIndikatorPin, HIGH);
  scale.power_up();
  float berat = scale.get_units();
  Serial.print(berat);
  Serial.println(" gram");
  Blynk.virtualWrite(V2, berat);
  scale.power_down();
  digitalWrite(ledIndikatorPin, LOW);
}

void setup() {
  // Debug console
  Serial.begin(115200);
  pinMode(ledIndikatorPin, OUTPUT);
  digitalWrite(ledIndikatorPin, HIGH);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  pinMode(pirPin, INPUT);

  initScale(73.89);

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
