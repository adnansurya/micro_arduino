#include "ACS712.h"
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

/*
  This example shows how to measure the power consumption
  of devices in 230V electrical system
  or any other system with alternative current
*/

// We have 30 amps version sensor connected to A1 pin of arduino
// Replace with your version if necessary
ACS712 sensor(ACS712_30A, A0);

void setup() {
  
  Serial.begin(9600);



  // This method calibrates zero point of sensor,
  // It is not necessary, but may positively affect the accuracy
  // Ensure that no current flows through the sensor at this moment
  
  sensor.calibrate();

  lcd.init();  // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();
}

void loop() {
  // We use 230V because it is the common standard in European countries
  // Change to your local, if necessary
  float U = 220;

  // To measure current we need to know the frequency of current
  // By default 50Hz is used, but you can specify own, if necessary
  float I = sensor.getCurrentAC();

  // To calculate the power we need voltage multiplied by current
  float P = U * I;

  // Serial.println(String("I = ") + I + " Ampere");
  Serial.println(String("P = ") + P + " Watts");
  lcd.setCursor(0, 0);
  lcd.print("I = ");
  lcd.print(I);
  lcd.print(" Amp");
  lcd.setCursor(0, 1);
  lcd.print("P = ");
  lcd.print(P);
  lcd.print(" Watt");

  delay(1000);
}