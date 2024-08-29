#include "ACS712.h"

#define sensor1Pin A1
#define sensor2Pin A2
#define sensor3Pin A3

ACS712 sensor1(ACS712_05B, sensor1Pin);
ACS712 sensor2(ACS712_05B, sensor2Pin);
ACS712 sensor3(ACS712_05B, sensor3Pin);

float current1, current2, current3;

void setup() {
  Serial.begin(9600);
  Serial.println("START");

  sensor1.calibrate();
  sensor2.calibrate();
  sensor3.calibrate();
  Serial.println("Calibrating Sensor OK");
}

void loop() {
  current1 = sensor1.getCurrentAC();
  current2 = sensor2.getCurrentAC();
  current3 = sensor3.getCurrentAC();

  // Send it to serial
  Serial.print("I_1 = ");
  Serial.print(current1);
  Serial.print(" A\t");
  Serial.print("I_2 = ");
  Serial.print(current2);
  Serial.print(" A\t");
  Serial.print("I_3 = ");
  Serial.print(current3);
  Serial.println(" A");

  // Wait one second before the new cycle
  delay(1000);
}
