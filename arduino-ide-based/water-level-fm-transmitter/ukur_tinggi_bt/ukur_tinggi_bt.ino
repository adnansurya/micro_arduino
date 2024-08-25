#include <NewPing.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

#define TRIGGER_PIN 26
#define ECHO_PIN 25
#define MAX_DISTANCE 200

long tinggiMax = 80;
long jarakUkur, tinggiAir;
long batasTinggi = 60;

int perintahInt;


NewPing sonar(TRIGGER_PIN, ECHO_PIN, tinggiMax);


void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32 Water");
  delay(100);

  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  SerialBT.println();
  SerialBT.println(F("DFRobot DFPlayer Mini Demo"));
  SerialBT.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  SerialBT.println(F("DFPlayer Mini online."));
  Serial.println(F("DFPlayer Mini online."));
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

  if (tinggiAir > batasTinggi) {
    Serial.println("BAHAYA!");
    SerialBT.println("BAHAYA!");
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