#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

DFRobotDFPlayerMini mp3;

void setup() {

  Serial2.begin(9600);
  Serial.begin(115200);
  SerialBT.begin("ESP32test");  //Bluetooth device name
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
    int perintahInt = perintah.toInt();

    if (perintahInt > 0) {
      mp3.play(perintahInt);
      SerialBT.print("Memainkan Track ke : ");
      SerialBT.println(perintahInt);
    } else {
      if (perintah == "next") {
        mp3.next();
        SerialBT.println("Track Selanjutnya");
      } else if (perintah == "prev") {
        mp3.previous();
        SerialBT.println("Track Sebelumnya");
      } else if (perintah == "pause") {
        mp3.pause();
        SerialBT.println("Pemutar Berhenti");
      } else if (perintah == "play") {
        mp3.start();
        SerialBT.println("Pemutar Dimainkan");
      } else if (perintah == "+") {
        mp3.volumeUp();
        SerialBT.print("Volume Naik ke : ");
        SerialBT.println(mp3.readVolume());
      } else if (perintah == "-") {
        mp3.volumeDown();
        SerialBT.print("Volume Turun ke : ");
        SerialBT.println(mp3.readVolume());
      }
    }
  }
}
