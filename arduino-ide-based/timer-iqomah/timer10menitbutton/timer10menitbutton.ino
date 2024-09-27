#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial14.h>
#include <fonts/SystemFont5x7.h>
#include <fonts/Calibri10.h>
#include <fonts/CourierNew14b.h>
#include <Wire.h>  // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);


#define buttonPin A0  // Pin tombol
#define buzzerPin 10  // Pin buzzer

unsigned long startMillis;    // Variabel untuk menyimpan waktu mulai
unsigned long currentMillis;  // Variabel untuk menyimpan waktu saat ini
unsigned long previousMillis = 0;
// const unsigned long period = 600000;  // 10 menit dalam milidetik (600000 ms = 10 menit)
const unsigned long period = 480000;  // 8 menit dalam milidetik (600000 ms = 10 menit)
// const unsigned long period = 30000;   // 30 detik dalam milidetik (600000 ms = 10 menit)
const unsigned long interval = 1000;  // 10 menit dalam milidetik (600000 ms = 10 menit)
int coundDownStart = 10;
bool timerRunning = false;   // Status apakah timer sedang berjalan
bool timerFinished = false;  // Status apakah timer sudah selesai

const char* MESSAGE = "Waktunya melaksanakan sholat";
String* dateTimeStrings;
String currentClock = "00:00";
String lastClock = "00:00";


SoftDMD dmd(1, 1);  // DMD controls the entire display
// SPIDMD dmd(1,1);  // DMD controls the entire display
DMD_TextBox box(dmd, 0, 1);  // "box" provides a text box to automatically write to/scroll the display

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

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);  // Mengatur pin tombol dengan pull-up internal
  pinMode(buzzerPin, OUTPUT);
  bunyi(100, 10);
  Serial.begin(9600);  // Memulai komunikasi serial
  Serial.println("Standby, tekan tombol untuk memulai timer.");

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  //--------RTC SETUP ------------
  // if you are using ESP-01 then uncomment the line below to reset the pins to
  // the available pins for SDA, SCL
  // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL

  Rtc.Begin();
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000 /* us */, true /* reset_on_timeout */);
#endif

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  String* compDateTimeStrings = getDateTimeStrings(compiled);

  Serial.print("Date: ");
  Serial.println(compDateTimeStrings[0]);
  Serial.print("Time: ");
  Serial.println(compDateTimeStrings[1]);
  Serial.println();
  Serial.println();

  if (!Rtc.IsDateTimeValid()) {
    if (!wasError("setup IsDateTimeValid")) {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing

      Serial.println("RTC lost confidence in the DateTime!");

      // following line sets the RTC to the date & time this sketch was compiled
      // it will also reset the valid flag internally unless the Rtc device is
      // having an issue

      Rtc.SetDateTime(compiled);
    }
  }

  if (!Rtc.GetIsRunning()) {
    if (!wasError("setup GetIsRunning")) {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
    }
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (!wasError("setup GetDateTime")) {
    if (now < compiled) {
      Serial.println("RTC is older than compile time, updating DateTime");
      Rtc.SetDateTime(compiled);
    } else if (now > compiled) {
      Serial.println("RTC is newer than compile time, this is expected");
    } else if (now == compiled) {
      Serial.println("RTC is the same as compile time, while not expected all is still fine");
    }
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  wasError("setup Enable32kHzPin");
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
  wasError("setup SetSquareWavePin");

  dmd.setBrightness(255);
  dmd.selectFont(CourierNew14b);
  dmd.begin();
  delay(2000);
  beep(2, 0.1);
}

void loop() {

  currentMillis = millis();  // Dapatkan waktu saat ini

  // Membaca status tombol
  if (digitalRead(buttonPin) == LOW) {  // Tombol ditekan (LOW karena pull-up)
    if (!timerRunning) {
      startMillis = currentMillis;  // Simpan waktu mulai
      timerRunning = true;          // Mengaktifkan timer
      timerFinished = false;        // Reset status selesai
      Serial.println("Timer dimulai!");
      bunyi(100, 0);
      delay(200);  // Debouncing tombol
    }
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    updateTime();
    currentClock = dateTimeStrings[1].substring(0, 5);
    int seconds = dateTimeStrings[1].substring(7).toInt();
    if (seconds % 2 == 0) {
      currentClock.replace(":", " ");
    }
    Serial.print("Current : ");
    Serial.println(currentClock);
    Serial.print("Seconds: ");
    Serial.println(seconds);

    if (timerRunning) {


      if (currentMillis - startMillis < period) {



        unsigned long remainingTime = period - (currentMillis - startMillis);  // Waktu tersisa dalam milidetik
        unsigned int minutes = remainingTime / 60000;                          // Konversi ke menit
        unsigned int seconds = (remainingTime % 60000) / 1000;                 // Konversi ke detik

        Serial.print("Sisa waktu: ");
        Serial.print(minutes);
        Serial.print(':');
        Serial.println(seconds);

        if (minutes == 0 && seconds <= coundDownStart) {
          bunyi(100, 0);

          dmd.clearScreen();
          dmd.selectFont(CourierNew14b);


          char secondStr[6];  // "MM:SS\0" -> 5 characters + 1 null terminator

          snprintf_P(secondStr,
                     sizeof(secondStr),
                     PSTR("%u:%02u"),
                     minutes,
                     seconds);

          dmd.drawString(6, 2, secondStr);
          dmd.drawBox(0, 0, 31, 15);


        } else {
          dmd.clearScreen();


          dmd.selectFont(Calibri10);

          char secondStr[6];  // "MM:SS\0" -> 5 characters + 1 null terminator

          snprintf_P(secondStr,
                     sizeof(secondStr),
                     PSTR("%02u:%02u"),
                     minutes,
                     seconds);
          dmd.drawString(4, 8, secondStr);

          dmd.drawString(1, -1, "iqomah ");
        }


      } else {
        Serial.println("Timer selesai!");
        digitalWrite(buzzerPin, HIGH);
        dmd.clearScreen();

        dmd.selectFont(Arial14);
        const char* next = MESSAGE;
        int toneLimit = 4;
        int toneCounter = 0;
        while (*next) {
          if (toneCounter >= toneLimit) {
            digitalWrite(buzzerPin, LOW);
          }
          Serial.print(*next);
          box.print(*next);
          delay(200);
          next++;
          toneCounter++;
        }
        timerRunning = false;  // Matikan timer setelah selesai
        timerFinished = true;  // Tandai bahwa timer sudah selesai
        delay(2000);
      }
    } else {
      if (currentClock != lastClock) {
        dmd.clearScreen();
        if (dateTimeStrings[1] == "00:00:00") {
          dmd.selectFont(Arial14);
          bunyi(50, 0);
          const char* next2 = "Memasuki tengah malam....     by: MAKASSAR ROBOTICS ";
          while (*next2) {
            Serial.print(*next2);
            box.print(*next2);
            delay(200);
            next2++;
          }
        } else {
          dmd.selectFont(SystemFont5x7);
          dmd.drawString(1, 4, currentClock.c_str());
        }
      }
    }
    lastClock = currentClock;
  }

  delay(10);
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
String* getDateTimeStrings(const RtcDateTime& dt) {
  static String result[2];  // array to hold date and time strings

  char dateStr[11];  // "MM/DD/YYYY\0" -> 10 characters + 1 null terminator
  char timeStr[9];   // "HH:MM\0" -> 8 characters + 1 null terminator

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

void bunyi(int nyala, int mati) {
  digitalWrite(buzzerPin, HIGH);
  delay(nyala);
  digitalWrite(buzzerPin, LOW);
  delay(mati);
}

void beep(int ulang, float detik) {
  for (int i = 0; i < ulang; i++) {
    bunyi(detik * 1000, detik * 1000);
  }
}