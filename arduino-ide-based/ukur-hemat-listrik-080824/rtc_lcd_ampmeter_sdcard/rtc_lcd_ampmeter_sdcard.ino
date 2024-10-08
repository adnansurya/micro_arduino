#include <SPI.h>
#include <SD.h>
#include <RtcDS1302.h>
#include <LiquidCrystal_I2C.h>
#include "ACS712.h"

#define sensor1Pin A0
#define sensor2Pin A1
#define sensor3Pin A2
#define sensor4Pin A3

#define csPin 53

ACS712 sensor1(ACS712_05B, sensor1Pin);
ACS712 sensor2(ACS712_05B, sensor2Pin);
ACS712 sensor3(ACS712_05B, sensor3Pin);
ACS712 sensor4(ACS712_05B, sensor4Pin);

float current1, current2, current3, current4;

ThreeWire myWire(3, 2, 4);  // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

unsigned long lcdUpdateTime = 0;
unsigned long lcdUpdateInterval = 1000;

String* dateTimeStrings;

int updateCounter = 0;
int savingTimer = 5;

File myFile;
String separator = ";";


void setup() {
  Serial.begin(9600);

  while (!Serial)
    ;

  lcdInit();

  sdCardInit();

  rtcInit();

  sensorInit();
}

void loop() {

  unsigned long currentTime = millis();

  if (currentTime - lcdUpdateTime >= lcdUpdateInterval) {
    getSensorData();

    if (updateCounter < savingTimer) {
      displaySensorData();

    } else {

      updateTime();
      displayTime();
      writeData();
      updateCounter = 0;

      delay(2000);
    }

    updateCounter++;
    lcdUpdateTime = currentTime;
  }
}


void writeData() {
  // Buat string data dalam format CSV
  String dataString = dateTimeStrings[0] + separator + dateTimeStrings[1] + separator + String(current1) + separator + String(current2) + separator + String(current3) + separator + String(current4);

  // Tulis data ke file CSV
  myFile = SD.open("log.csv", FILE_WRITE);
  if (myFile) {
    myFile.println(dataString);
    myFile.close();
    Serial.println("Data written to SD: " + dataString);
  } else {
    Serial.println("Error opening current_log.csv");
  }
}

void sdCardInit() {

  if (!SD.begin(csPin)) {
    Serial.println("Card failed, or not present");
    while (1)
      ;  // Berhenti jika SD card tidak tersedia
  }
  Serial.println("Card initialized.");
}

void displayTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DateTime: ");
  lcd.setCursor(0, 1);
  lcd.print(dateTimeStrings[0]);
  lcd.setCursor(0, 2);
  lcd.print(dateTimeStrings[1]);
  lcd.setCursor(0, 3);
  lcd.print("Saving Data...");
}


void updateTime() {

  RtcDateTime now = Rtc.GetDateTime();
  dateTimeStrings = getDateTimeStrings(now);
  if (!now.IsValid()) {
    Serial.println("RTC lost confidence in the DateTime!");
  }

  Serial.print("Date: ");
  Serial.println(dateTimeStrings[0]);
  Serial.print("Time: ");
  Serial.println(dateTimeStrings[1]);
}

void sensorInit() {
  sensor1.calibrate();
  sensor2.calibrate();
  sensor3.calibrate();
  sensor4.calibrate();
  Serial.println("Calibrating Sensors OK");
}

void displaySensorData() {
  lcd.clear();
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
}

void getSensorData() {
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
}


void rtcInit() {
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  String* compDateTimeStrings = getDateTimeStrings(compiled);

  Serial.print("Date: ");
  Serial.println(compDateTimeStrings[0]);
  Serial.print("Time: ");
  Serial.println(compDateTimeStrings[1]);
  Serial.println();
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
    //  Rtc.SetDateTime(compiled);
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }


  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);
}

void lcdInit() {

  lcd.init();  // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Initializing..");
}

String* getDateTimeStrings(const RtcDateTime& dt) {
  static String result[2];  // array to hold date and time strings

  char dateStr[11];  // "MM/DD/YYYY\0" -> 10 characters + 1 null terminator
  char timeStr[9];   // "HH:MM:SS\0" -> 8 characters + 1 null terminator

  snprintf_P(dateStr,
             sizeof(dateStr),
             PSTR("%02u/%02u/%04u"),
             dt.Month(),
             dt.Day(),
             dt.Year());

  snprintf_P(timeStr,
             sizeof(timeStr),
             PSTR("%02u:%02u:%02u"),
             dt.Hour(),
             dt.Minute(),
             dt.Second());

  result[0] = String(dateStr);
  result[1] = String(timeStr);

  return result;
}