
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

  for(int u = 0; u < 10; u++){
     beriMakan();
  }
  myservo.write(servoPin, 90); 
  delay(5000);
    

 
  
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
