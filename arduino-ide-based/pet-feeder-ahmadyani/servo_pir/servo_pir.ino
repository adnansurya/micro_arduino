
#include <Servo.h>

#define ledIndikatorPin 2
#define pirPin 19

Servo myservo = Servo();

const int servoPin = 23;

int adaGerak = 0;
int lastGerak = 0;

void setup() {
  Serial.begin(115200);

  pinMode(ledIndikatorPin, OUTPUT);
  digitalWrite(ledIndikatorPin, LOW);

  pinMode(pirPin, INPUT);
}

void loop() {

  adaGerak = digitalRead(pirPin);
  Serial.print("SENSOR : ");
  Serial.println(adaGerak);

  if (lastGerak != adaGerak) {
    if (adaGerak == 1) {
      Serial.println("GERAKAN TERDETEKSI !");
      beriMakan(10);
      // delay(2000);
    } else {
      delay(500);
    }
  }
  lastGerak = adaGerak;
  delay(200);

  // beriMakan();
}

void beriMakan(int ulang) {
  for (int u = 0; u < ulang; u++) {
    for (int pos = 0; pos <= 180; pos++) {  // go from 0-180 degrees
      myservo.write(servoPin, pos);         // set the servo position (degrees)
      delay(1);
    }
    for (int pos = 180; pos >= 0; pos--) {  // go from 180-0 degrees
      myservo.write(servoPin, pos);         // set the servo position (degrees)
      delay(1);
    }
  }
}
