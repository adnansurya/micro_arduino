#include <SPI.h>
#include <SD.h>
#include <Wire.h>  // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
#include <LiquidCrystal_I2C.h>
#include "ACS712.h"

#define current1Pin A0
#define current2Pin A1
#define current3Pin A2
#define current4Pin A3

#define csPin 53

#define motionPin 6

#define relay1Pin 8
#define relay2Pin 9
#define relay3Pin 10
#define relay4Pin 11

ACS712 acs1(ACS712_30A, current1Pin);
ACS712 acs2(ACS712_05B, current2Pin);
ACS712 acs3(ACS712_05B, current3Pin);
ACS712 acs4(ACS712_05B, current4Pin);

float current1, current2, current3, current4;

RtcDS3231<TwoWire> Rtc(Wire);

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

unsigned long lcdUpdateTime = 0;
unsigned long lcdUpdateInterval = 1000;

String* dateTimeStrings;

int updateCounter = 0;
int savingTimer = 5;

File myFile;
String separator = ";";

int motionDetected = 0;
int lastMotionDetected = 0;

bool relayOn = false;
unsigned long motionUpdateTime = 0;
unsigned long motionUpdateTimeOut = 10000;

bool wasError(const char* errorTopic = "") {
  uint8_t error = Rtc.LastError();
  if (error != 0) {
    // we have a communications error
    // see https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
    // for what the number means
    Serial.print("[");
    Serial.print(errorTopic);
    Serial.print("] WIRE communications error (");
    Serial.print(error);
    Serial.print(") : ");

    switch (error) {
      case Rtc_Wire_Error_None:
        Serial.println("(none?!)");
        break;
      case Rtc_Wire_Error_TxBufferOverflow:
        Serial.println("transmit buffer overflow");
        break;
      case Rtc_Wire_Error_NoAddressableDevice:
        Serial.println("no device responded");
        break;
      case Rtc_Wire_Error_UnsupportedRequest:
        Serial.println("device doesn't support request");
        break;
      case Rtc_Wire_Error_Unspecific:
        Serial.println("unspecified error");
        break;
      case Rtc_Wire_Error_CommunicationTimeout:
        Serial.println("communications timed out");
        break;
    }
    return true;
  }
  return false;
}


void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  lcdInit();

  pinMode(motionPin, INPUT_PULLUP);
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  pinMode(relay3Pin, OUTPUT);
  pinMode(relay4Pin, OUTPUT);

  powerOff();

  sdCardInit();

  rtcInit();

  sensorInit();
}

void loop() {



  unsigned long currentTime = millis();
  if (currentTime - lcdUpdateTime >= lcdUpdateInterval) {
    updateTime();

    if (relayOn) {
      getCurrentData();

      if (updateCounter < savingTimer) {
        displaySensorData();

      } else {
        writeData();
        lcd.setCursor(15, 3);
        lcd.print("SAVE");
        lcd.print((char)126);
        updateCounter = 0;
      }

      updateCounter++;
    } else {
      standbyDisplay();
    }

    lcdUpdateTime = currentTime;
  }



  motionDetected = digitalRead(motionPin);
  // Serial.print("Motion : ");
  // Serial.println(motionDetected);
  if (motionDetected == 1 && motionDetected != lastMotionDetected && relayOn == false) {
    relayOn = true;
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("Gerakan");
    lcd.setCursor(5, 1);
    lcd.print("Terdeteksi");
    lcd.setCursor(6, 3);
    lcd.print("POWER ON");
    delay(2000);
    powerOn();
  }


  if (relayOn) {
    if (motionDetected == true) {
      motionUpdateTime = currentTime;

      lcd.setCursor(16, 1);
      lcd.print("MOVE");


    } else {
      if (currentTime - motionUpdateTime >= motionUpdateTimeOut) {
        relayOn = false;

        updateCounter = 0;
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("Tidak Ada Gerakan");
        lcd.setCursor(3, 1);
        lcd.print("Selama ");
        lcd.print((motionUpdateTimeOut / 1000));
        lcd.print(" detik");
        lcd.setCursor(5, 3);
        lcd.print("POWER OFF!");
        powerOff();
        delay(2000);
      }
    }
  }
  lastMotionDetected = motionDetected;
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

void standbyDisplay() {
  lcd.clear();
  lcd.setCursor(7, 0);
  lcd.print("Status");
  lcd.setCursor(4, 1);
  if (relayOn) {
    lcd.print("< Power ON >");
  } else {
    lcd.print("< Power OFF>");
  }
  lcd.setCursor(5, 2);
  lcd.print(dateTimeStrings[0]);
  lcd.setCursor(6, 3);
  lcd.print(dateTimeStrings[1]);
}


void updateTime() {

  RtcDateTime now = Rtc.GetDateTime();
  dateTimeStrings = getDateTimeStrings(now);
  if (!now.IsValid()) {
    Serial.println("RTC lost confidence in the DateTime!");
  }

  // Serial.print("Date: ");
  // Serial.println(dateTimeStrings[0]);
  // Serial.print("Time: ");
  // Serial.println(dateTimeStrings[1]);
}

void sensorInit() {
  acs1.calibrate();
  acs2.calibrate();
  acs3.calibrate();
  acs4.calibrate();
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

void getCurrentData() {
  current1 = acs1.getCurrentAC();
  current2 = acs2.getCurrentAC();
  current3 = acs3.getCurrentAC();
  current4 = acs4.getCurrentAC();

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

  // RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  // String* compDateTimeStrings = getDateTimeStrings(compiled);

  // Serial.print("Date: ");
  // Serial.println(compDateTimeStrings[0]);
  // Serial.print("Time: ");
  // Serial.println(compDateTimeStrings[1]);
  // Serial.println();
  // Serial.println();

  if (!Rtc.IsDateTimeValid()) {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    // Rtc.SetDateTime(compiled);
  }


  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  // if (now < compiled) {
  //   Serial.println("RTC is older than compile time!  (Updating DateTime)");
  //   Rtc.SetDateTime(compiled);
  // } else if (now > compiled) {
  //   Serial.println("RTC is newer than compile time. (this is expected)");
  //   //  Rtc.SetDateTime(compiled);
  // } else if (now == compiled) {
  //   Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  // }

  Rtc.Enable32kHzPin(false);
  wasError("setup Enable32kHzPin");
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
  wasError("setup SetSquareWavePin");


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
             dt.Day(),
             dt.Month(),
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

void powerOff() {
  Serial.println("Power OFF!");
  digitalWrite(relay1Pin, HIGH);
  digitalWrite(relay2Pin, HIGH);
  digitalWrite(relay3Pin, HIGH);
  digitalWrite(relay4Pin, HIGH);
}

void powerOn() {
  Serial.println("Power ON!");
  digitalWrite(relay1Pin, LOW);
  digitalWrite(relay2Pin, LOW);
  digitalWrite(relay3Pin, LOW);
  digitalWrite(relay4Pin, LOW);
}