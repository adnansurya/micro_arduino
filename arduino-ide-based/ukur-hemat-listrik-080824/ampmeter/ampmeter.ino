#include "ACS712.h"

#define sensor1Pin A1
#define sensor2Pin A2
#define sensor3Pin A3

ACS712 sensor1(ACS712_05B, sensor1Pin);
ACS712 sensor2(ACS712_05B, sensor2Pin);
ACS712 sensor3(ACS712_05B, sensor3Pin);

void setup() {
  Serial.begin(9600);
  Serial.println("START");

  sensor1.calibrate();
  sensor2.calibrate();
  sensor3.calibrate();
  Serial.println("Calibrating Sensor OK");  
}

void loop() {
}
