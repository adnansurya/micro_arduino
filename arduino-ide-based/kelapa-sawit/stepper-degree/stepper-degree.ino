/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-stepper-motor-28byj-48-uln2003/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Based on Stepper Motor Control - one revolution by Tom Igoe
*/
#include <Stepper.h>

const int stepsPerRevolution = 2048;  // change this to fit the number of steps per revolution
const float resolution = 0.17578125;  // put your step resolution here
float currentAngle = 0.0;

// ULN2003 Motor Driver Pins
#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17

// initialize the stepper library
Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

void setup() {
  // set the speed at 5 rpm
  myStepper.setSpeed(8);
  // initialize the serial port
  Serial.begin(115200);
}

void loop() {
  // step one revolution in one direction:
  Serial.println("clockwise");
  myStepper.step(stepsPerRevolution);
  delay(1000);

  // step one revolution in the other direction:
  Serial.println("counterclockwise");
  myStepper.step(-stepsPerRevolution);
  delay(1000);

  Serial.println("60");
  myStepper.step(step_degree(60));
  delay(1000);

  Serial.println("120");
  myStepper.step(step_degree(120));
  delay(1000);

  Serial.println("180");
  myStepper.step(step_degree(180));
  delay(1000);

  Serial.println("240");
  myStepper.step(step_degree(240));
  delay(1000);

  Serial.println("70");
  myStepper.step(step_degree(70));
  delay(1000);

  Serial.println("300");
  myStepper.step(step_degree(300));
  delay(1000);

  Serial.println("360");
  myStepper.step(step_degree(360));
  delay(1000);
}


int step_degree(float desired_degree) {
  Serial.print("CURRENT : ");
  Serial.println(currentAngle);
  int step_moved = (desired_degree - currentAngle) / resolution;
  currentAngle = desired_degree;
  return step_moved;
}