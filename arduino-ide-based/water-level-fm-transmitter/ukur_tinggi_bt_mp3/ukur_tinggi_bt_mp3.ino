#include <NewPing.h>
#include "BluetoothSerial.h"
#include "DFRobotDFPlayerMini.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
DFRobotDFPlayerMini mp3;


#define TRIGGER_PIN 21
#define ECHO_PIN 22
#define MAX_DISTANCE 200

long tinggiMax = 80;
long jarakDeteksi = 5;
long jarakUkur, tinggiAir;
long batasTinggi = 60;

int perintahInt;


NewPing sonar(TRIGGER_PIN, ECHO_PIN, tinggiMax);


void setup() {
  Serial.begin(115200);
  Serial2.begin(9600);
  SerialBT.begin("ESP32 Water");
  delay(100);

  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  SerialBT.println();
  SerialBT.println(F("DFRobot DFPlayer Mini Demo"));
  SerialBT.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!mp3.begin(Serial2)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));

    SerialBT.println(F("Unable to begin:"));
    SerialBT.println(F("1.Please recheck the connection!"));
    SerialBT.println(F("2.Please insert the SD card!"));
    while (true)
      ;
  }

  SerialBT.println(F("DFPlayer Mini online."));
  Serial.println(F("DFPlayer Mini online."));

  mp3.setTimeOut(500);  //Set serial communictaion time out 500ms

  //----Set volume----
  mp3.volume(20);  //Set volume value (0~30).

  //----Set different EQ----
  mp3.EQ(DFPLAYER_EQ_NORMAL);
}

void loop() {

  if (SerialBT.available() > 0) {
    String perintah = SerialBT.readStringUntil('\n');
    perintah.trim();
    perintahInt = perintah.toInt();
  } else {
    delay(500);
  }

  jarakUkur = sonar.ping_cm();

  tinggiAir = tinggiMax - jarakUkur;

  if (tinggiAir > batasTinggi && tinggiAir < (tinggiMax - jarakDeteksi)) {
    Serial.println("PERINGATAN!");
    SerialBT.println("PERINGATAN!");
    mp3.play(2);
    delay(3000);
  } else if (tinggiAir > batasTinggi && tinggiAir >= (tinggiMax - jarakDeteksi)) {
    Serial.println("BAHAYA!");
    SerialBT.println("BAHAYA!");
    mp3.play(1);
    delay(16000);
  }

  Serial.print("Jarak Ukur: ");
  Serial.print(jarakUkur);
  Serial.print(" cm\t");
  Serial.print("Tinggi Air : ");
  Serial.print(tinggiAir);
  Serial.println("cm");


  SerialBT.print("Jarak Ukur: ");
  SerialBT.print(jarakUkur);
  SerialBT.println(" cm\t");
  SerialBT.print("Tinggi Air : ");
  SerialBT.print(tinggiAir);
  SerialBT.println("cm");
}