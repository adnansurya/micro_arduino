#include <NewPing.h>
#include "BluetoothSerial.h"
#include "HardwareSerial.h"
#include "DFRobotDFPlayerMini.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
DFRobotDFPlayerMini mp3;
HardwareSerial dfSD(1); // Use UART channel 1

#define RXD2 16 // Connects to module's RX 
#define TXD2 17 // Connects to module's TX 

#define buzzerPin 14

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
  pinMode(buzzerPin, OUTPUT);
  beep(1, 0.2);
  Serial.begin(115200);
  dfSD.begin(9600, SERIAL_8N1, RXD2, TXD2);  // 16,17
  SerialBT.begin("ESP32 Water");
  delay(100);

  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  SerialBT.println();
  SerialBT.println(F("DFRobot DFPlayer Mini Demo"));
  SerialBT.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!mp3.begin(dfSD))  { //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));

    SerialBT.println(F("Unable to begin:"));
    SerialBT.println(F("1.Please recheck the connection!"));
    SerialBT.println(F("2.Please insert the SD card!"));
    while (true)
      beep(1, 0.5);
      
  }

  SerialBT.println(F("DFPlayer Mini online."));
  Serial.println(F("DFPlayer Mini online."));

  mp3.setTimeOut(500);  //Set serial communictaion time out 500ms

  //----Set volume----
  mp3.volume(20);  //Set volume value (0~30).

  //----Set different EQ----
  mp3.EQ(DFPLAYER_EQ_NORMAL);
  beep(2, 0.2);
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
    // beep(3, 0.2);
    mp3.play(2);
    delay(3000);
  } else if (tinggiAir > batasTinggi && tinggiAir >= (tinggiMax - jarakDeteksi)) {
    Serial.println("BAHAYA!");
    SerialBT.println("BAHAYA!");
    // beep(5, 0.2);
    mp3.play(1);
    delay(3000);
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

void beep(int ulang, float detik) {
  for (int i = 0; i < ulang; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(detik * 1000);
    digitalWrite(buzzerPin, LOW);
    delay(detik * 1000);
  }
}