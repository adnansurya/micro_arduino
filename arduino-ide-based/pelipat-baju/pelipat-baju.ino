/**
 * Complete project details at https://RandomNerdTutorials.com/arduino-load-cell-hx711/
 *
 * HX711 library for Arduino - example file
 * https://github.com/bogde/HX711
 *
 * MIT License
 * (c) 2018 Bogdan Necula
 *
**/
#include <Servo.h>
#include <Arduino.h>
#include "HX711.h"

#define servo1Pin 5
#define servo2Pin 6
#define servo3Pin 9
#define servo4Pin 10
#define LEDPin 13  //Led default dari Arduino uno


#define LOADCELL_DOUT_PIN 2
#define LOADCELL_SCK_PIN 3

const int jedaLipat = 1000;  //milisekon
const int sudutLipat = 200;

Servo servo1, servo2, servo3, servo4;

HX711 scale;

const float batasBerat = 30.0;

int sudutKiri = 120;
int sudutKanan = 150;
int sudutBawah = 140;
int sudutAtas = 140;

void setup() {
  Serial.begin(57600);
  pinMode(LEDPin, OUTPUT);
     digitalWrite(LEDPin, HIGH);
  Serial.println("HX711 Demo");
  Serial.println("Initializing the scale");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

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

  scale.set_scale(400.364);
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


  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo3.attach(servo3Pin);
  servo4.attach(servo4Pin);
  resetServo();
  digitalWrite(LEDPin, LOW);
}

void loop() {
  Serial.print("Berat:\t");

  float berat = scale.get_units();
  Serial.print(berat);
  Serial.println(" gram");

  if (berat >= batasBerat) {
    lipat(jedaLipat);
  } else {
    digitalWrite(LEDPin, LOW);
  }
  delay(5000);
}





void lipat(int jeda) {

  Serial.println("LIPAT");
  digitalWrite(LEDPin, HIGH);
  servo1.write(sudutKiri);
  delay(jeda);
  servo1.write(0);
  delay(jeda);
  servo2.write(sudutKanan);
  delay(jeda);
  servo2.write(0);
  delay(jeda);
  servo3.write(sudutBawah);
  delay(jeda);
  servo3.write(0);
  delay(jeda);
  servo4.write(sudutAtas);
  delay(jeda);
  servo4.write(0);
  delay(jeda);
  resetServo();
}

void resetServo() {
  servo1.write(0);
  servo2.write(0);
  servo3.write(0);
  servo4.write(0);
}