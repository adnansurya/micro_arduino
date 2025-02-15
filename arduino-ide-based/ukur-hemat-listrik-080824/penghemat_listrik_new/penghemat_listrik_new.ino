#include <SPI.h>
#include <SD.h>
#include <Wire.h>  // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
#include <LiquidCrystal_I2C.h>
#include "ACS712.h"

#define current1Pin A0
#define current2Pin A1
#define current3Pin A3
#define current4Pin A5

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
float power1, power2, power3, power4;

float cal1, cal2, cal3, cal4;

bool calibrate = false;

RtcDS3231<TwoWire> Rtc(Wire);

LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

unsigned long lcdUpdateTime = 0;
unsigned long lcdUpdateInterval = 3000;
int lcdCounter = 0;
int lcdLimit = 3;
bool displayPower = false;

String* dateTimeStrings;

int updateCounter = 0;
int savingTimer = 5;

File myFile;
String separator = ";";

int motionDetected = 0;
int lastMotionDetected = 0;

bool relayOn = false;
unsigned long motionUpdateTime = 0;
unsigned long motionUpdateTimeOut = 20 * 1000;

unsigned long switchOnTimeSecond = 0;
unsigned long switchOffTimeSecond = 0;
unsigned long onDurationTimeSecond = 0;

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

  delay(1000);

  sensorInit();

  delay(1000);

  callibrateACS(50);

  calibrate = true;
}

void loop() {



  unsigned long currentTime = millis();
  if (currentTime - lcdUpdateTime >= lcdUpdateInterval) {


    updateTime();

    if (relayOn) {
      if (!displayPower) {
        getCurrentData();
      }

      lcdUpdateInterval = 3000;

      displaySensorData();

    } else {
      standbyDisplay();
      lcdUpdateInterval = 1000;
    }

    lcdUpdateTime = currentTime;
    lcdCounter++;
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
    switchOnTimeSecond = millis() / 1000;
    Serial.println(switchOnTimeSecond);
    String dataString = dateTimeStrings[0] + separator + dateTimeStrings[1] + separator
                        + "SWITCH ON" + separator + "-" + separator + "-" + separator + "-" + separator
                        + "-" + separator + "-" + separator + "-" + separator + "-";
    writeData(dataString);
  }


  if (relayOn) {
    if (motionDetected == true) {
      motionUpdateTime = currentTime;

      lcd.setCursor(16, 2);
      lcd.print("MOVE");


    } else {
      if (currentTime - motionUpdateTime >= motionUpdateTimeOut) {
        relayOn = false;

        powerOff();
        switchOffTimeSecond = millis() / 1000;
        Serial.println(switchOffTimeSecond);
        onDurationTimeSecond = switchOffTimeSecond - switchOnTimeSecond;
        Serial.println(onDurationTimeSecond);

        float durationHours = (float)onDurationTimeSecond / 3600.0;
        String dataString = dateTimeStrings[0] + separator + dateTimeStrings[1] + separator
                            + String(current1) + separator + String(current2) + separator + String(current3) + separator + String(current4) + separator
                            + String(power1) + separator + String(power2) + separator + String(power3) + separator + String(power4) + separator
                            + String(onDurationTimeSecond) + separator + String(durationHours);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Active Duration: ");
        lcd.setCursor(1, 1);
        lcd.print(onDurationTimeSecond);
        lcd.print(" sec (");
        lcd.print(durationHours);
        lcd.print(" h)");
        lcd.setCursor(0, 3);
        lcd.print("POWER OFF! (");
        lcd.print((motionUpdateTimeOut / 1000));
        lcd.print(" sec)");

        writeData(dataString);
        delay(4000);
      }
    }
  }
  lastMotionDetected = motionDetected;
}


void writeData(String text) {
  // Tulis data ke file CSV
  myFile = SD.open("log.csv", FILE_WRITE);
  if (myFile) {
    myFile.println(text);
    myFile.close();
    Serial.println("Data written to SD: " + text);
  } else {
    displayError("Write Data Error");
    Serial.println("Error opening log.csv");
  }
}

void sdCardInit() {

  if (!SD.begin(csPin)) {

    Serial.println("Card failed, or not present");
    displayError("SD Card failed");
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
    displayError("RTC Time Error");
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
  displayPower = !displayPower;
  lcd.clear();
  if (displayPower) {
    lcd.setCursor(0, 0);
    lcd.print("P_1 = ");
    lcd.print(power1);
    lcd.print(" VA");
    lcd.setCursor(0, 1);
    lcd.print("P_2 = ");
    lcd.print(power2);
    lcd.print(" VA");
    lcd.setCursor(0, 2);
    lcd.print("P_3 = ");
    lcd.print(power3);
    lcd.print(" VA");
    lcd.setCursor(0, 3);
    lcd.print("P_4 = ");
    lcd.print(power4);
    lcd.print(" VA");

  } else {
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
}

void getCurrentData() {


  current1 = String(acs1.getCurrentAC()).toFloat();
  current2 = String(acs2.getCurrentAC()).toFloat();
  current3 = String(acs3.getCurrentAC()).toFloat();
  current4 = String(acs4.getCurrentAC()).toFloat();



  if (calibrate) {
    current1 = current1 - cal1;
    current2 = current2 - cal2;
    current3 = current3 - cal3;
    current4 = current4 - cal4;

    if (current1 < 0.0) {
      current1 = 0.0;
    }
    if (current2 < 0.0) {
      current2 = 0.0;
    }
    if (current3 < 0.0) {
      current3 = 0.0;
    }
    if (current4 < 0.0) {
      current4 = 0.0;
    }

    power1 = current1 * 220.0;
    power2 = current2 * 220.0;
    power3 = current3 * 220.0;
    power4 = current4 * 220.0;
  }

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

  if (calibrate) {
    // Send it to serial
    Serial.print("P_1 = ");
    Serial.print(power1);
    Serial.print(" W\t");
    Serial.print("P_2 = ");
    Serial.print(power2);
    Serial.print(" W\t");
    Serial.print("P_3 = ");
    Serial.print(power3);
    Serial.print(" W\t");
    Serial.print("P_4 = ");
    Serial.print(power4);
    Serial.println(" W");
    Serial.println();
  }
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
    displayError("RTC DateTime Error");
  }


  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    displayError("RTC Not Running");
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


void displayError(String text) {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(text);
  delay(2500);
}

void callibrateACS(int sample) {

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Callibrating..");

  for (int i = 0; i < sample; i++) {
    getCurrentData();
    if (current1 > cal1) {
      cal1 = current1;
    }
    if (current2 > cal2) {
      cal2 = current2;
    }
    if (current3 > cal3) {
      cal3 = current3;
    }
    if (current4 > cal4) {
      cal4 = current4;
    }
    delay(100);
  }
}
