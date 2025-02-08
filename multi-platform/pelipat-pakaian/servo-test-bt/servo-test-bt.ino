#include <Servo.h>

Servo myservo;  // create servo object to control a servo

int angle;  // variable to store the angle from serial input

void setup() {
  Serial.begin(9600);  // start serial communication at 9600bps
  myservo.attach(9);   // attaches the servo on pin 9 to the servo object
  Serial.println("Enter angle between 0 and 180:");
}

void loop() {
  if (Serial.available() > 0) {
    angle = Serial.parseInt();  // read the integer from serial input
    if (angle >= 0 && angle <= 180) {
      myservo.write(angle);  // set servo to angle
      Serial.print("Servo angle set to: ");
      Serial.println(angle);
    } else {
      Serial.println("Please enter a valid angle between 0 and 180.");
    }
  }
}
