#include <RBDdimmer.h>  //

//#define USE_SERIAL  SerialUSB //Serial for boards whith USB serial port

#define dimmer1Pin 14 //12
#define dimmer2Pin 13 //14
#define zerocrossPin 16  // for boards with CHANGEBLE input pins

//dimmerLamp dimmer(outputPin, zerocross); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards
dimmerLamp dimmer1(dimmer1Pin, zerocrossPin);
dimmerLamp dimmer2(dimmer2Pin, zerocrossPin);  //initialase port for dimmer for MEGA, Leonardo, UNO, Arduino M0, Arduino Zero

int outVal = 0;

void setup() {
  Serial.begin(9600);
  dimmer1.begin(NORMAL_MODE, ON);
  dimmer2.begin(NORMAL_MODE, ON);  //dimmer initialisation: name.begin(MODE, STATE)
}

void loop() {
  for (int i = 0; i < 100; i++) {
    dimmer1.setPower(i);
    Serial.print("P1 : ");
    Serial.println(dimmer1.getPower());
    delay(100);
  }
  for (int i = 0; i < 100; i++) {
    dimmer2.setPower(i);
    Serial.print("P2 : ");
    Serial.println(dimmer2.getPower());
    delay(100);
  }
  for (int i = 100; i > 0 ; i--) {
    dimmer1.setPower(i);
    Serial.print("P1 : ");
    Serial.println(dimmer1.getPower());
    delay(100);
  }
  for (int i = 100; i > 0; i--) {
    dimmer2.setPower(i);
    Serial.print("P2 : ");
    Serial.println(dimmer2.getPower());
    delay(100);
  }
}