#include "ACS712.h"

#define sensor1Pin A0
#define sensor2Pin A1
#define sensor3Pin A2
#define sensor4Pin A3

ACS712 sensor1(ACS712_05B, sensor1Pin);
ACS712 sensor2(ACS712_05B, sensor2Pin);
ACS712 sensor3(ACS712_05B, sensor3Pin);
ACS712 sensor4(ACS712_05B, sensor4Pin);

float current1, current2, current3, current4;

void setup() {
  Serial.begin(9600);
  Serial.println("START");

  sensor1.calibrate();
  sensor2.calibrate();
  sensor3.calibrate();
  sensor4.calibrate();
  Serial.println("Calibrating Sensor OK");
}

void loop() {
  current1 = sensor1.getCurrentAC();
  current2 = sensor2.getCurrentAC();
  current3 = sensor3.getCurrentAC();
  current4 = sensor4.getCurrentAC();

  // Send it to serial
  Serial.print("I_1 = ");
  Serial.print(current1);
  Serial.print(" A\t");
  Serial.print("I_2 = ");
  Serial.print(current2);
  Serial.print(" A\t");
  Serial.print("I_3 = ");
  Serial.print(current3);
  Serial.print(" A\t");
  Serial.print("I_4 = ");
  Serial.print(current4);
  Serial.println(" A");

  // Wait one second before the new cycle
  delay(1000);
}
