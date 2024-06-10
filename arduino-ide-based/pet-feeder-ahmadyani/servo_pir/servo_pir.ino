
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

  myservo.write(servoPin, 90);
  delay(2000);
}

void loop() {

  adaGerak = digitalRead(pirPin);
  Serial.print("SENSOR : ");
  Serial.println(adaGerak);

  if (lastGerak != adaGerak) {
    if (adaGerak == 1) {
      Serial.println("GERAKAN TERDETEKSI !");
      digitalWrite(ledIndikatorPin, HIGH);
      for (int u = 0; u < 20; u++) {
        beriMakan();
      }
      myservo.write(servoPin, 90);
      delay(2000);
      digitalWrite(ledIndikatorPin, LOW);
      
    } else {
      delay(500);
    }
  }
  lastGerak = adaGerak;
  delay(1000);

  // beriMakan();
}

void beriMakan() {

  for (int pos = 0; pos <= 180; pos++) {  // go from 0-180 degrees
    myservo.write(servoPin, pos);         // set the servo position (degrees)
    delay(1);
  }
  for (int pos = 180; pos >= 0; pos--) {  // go from 180-0 degrees
    myservo.write(servoPin, pos);         // set the servo position (degrees)
    delay(1);
  }
}
