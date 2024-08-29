#include <RtcDS1302.h>
#include <LiquidCrystal_I2C.h>
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

ThreeWire myWire(3, 2, 4);  // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

void setup() {
  Serial.begin(9600);

  lcd.init();  // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("initializing..");

  sensor1.calibrate();
  sensor2.calibrate();
  sensor3.calibrate();
  sensor4.calibrate();
  Serial.println("Calibrating Sensor OK");

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected()) {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
}

void loop() {
  lcd.clear();
  RtcDateTime now = Rtc.GetDateTime();

  printDateTime(now);
  Serial.println();

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

  // Send it to serial
  lcd.setCursor(0, 0);
  lcd.print("I_1 = ");
  lcd.print(current1);
  lcd.print(" A");
  lcd.setCursor(0, 1);
  lcd.print("I_2 = ");
  lcd.print(current2);
  lcd.print(" A");
  lcd.setCursor(0, 2);
  lcd.print("I_3 = ");
  lcd.print(current3);
  lcd.print(" A");
  lcd.setCursor(0, 3);
  lcd.print("I_4 = ");
  lcd.print(current4);
  lcd.print(" A");

  if (!now.IsValid()) {
    // Common Causes:
    //    1) the battery on the device is low or even missing and the power line was disconnected
    Serial.println("RTC lost confidence in the DateTime!");
  }

  delay(1000);  // ten seconds
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt) {
  char datestring[26];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  Serial.print(datestring);
}
