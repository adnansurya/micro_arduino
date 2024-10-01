#include <SoftwareSerial.h>

#define FORWARD 'F'
#define BACKWARD 'B'
#define LEFT 'L'
#define RIGHT 'R'
#define CIRCLE 'C'
#define CROSS 'X'
#define TRIANGLE 'T'
#define SQUARE 'S'
#define START 'A'
#define PAUSE 'P'
#define HALT 'H'


#define enA 5
#define in1 6
#define in2 7
#define in3 8
#define in4 9
#define enB 10

#define buzzerPin 13

SoftwareSerial mySerial(12, 11);  // RX, TX

int motorSpeed = 80;
int turningDelta = 40;
String currentState = "stop";

int actionVal = 25;
int maxSpeed = 200;
int minSpeed = 30;


void setup() {
  // Set all the motor control pins to outputs
  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Turn off motors - Initial state
  stop();

  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  mySerial.begin(9800);
}

void loop() {
  if (mySerial.available() > 0) {
    char command = mySerial.read();

    if (command != BACKWARD && command != FORWARD && String(command) != "0") {
      Serial.print("CHAR : ");
      Serial.println(String(command));
      executeCommand(command, motorSpeed);


    } else if (command == BACKWARD || command == FORWARD) {
      tuning(command);
    }else{
      stop();
    }



    // delay(1000);
  }
}




void stop() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  digitalWrite(buzzerPin, LOW);
  currentState = "Stop";
}


void backward(int pwm) {

  analogWrite(enA, pwm);
  analogWrite(enB, pwm);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
  currentState = "backward : " + String(pwm);
}

void forward(int pwm) {

  analogWrite(enA, pwm);
  analogWrite(enB, pwm);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  currentState = "forward : " + String(pwm);
}

void turnLeft(int pwm) {
  analogWrite(enA, pwm);
  analogWrite(enB, pwm - turningDelta);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  currentState = "turnLeft : " + String(pwm);
}

void turnRight(int pwm) {
  analogWrite(enA, pwm - turningDelta);
  analogWrite(enB, pwm);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  currentState = "turnRight : " + String(pwm);
}

void speedDown() {
  if (motorSpeed - actionVal >= minSpeed) {
    motorSpeed = motorSpeed - actionVal;
  } else {
    motorSpeed = minSpeed;
    bunyi(5);
  }

  Serial.print("Speed Down : ");
  Serial.println(motorSpeed);
}

void speedUp() {
  if (motorSpeed + actionVal <= maxSpeed) {
    motorSpeed = motorSpeed + actionVal;

  } else {
    motorSpeed = maxSpeed;
    bunyi(5);
  }

  Serial.print("Speed Up : ");
  Serial.println(motorSpeed);
}

void executeCommand(char command, int pwm) {
  Serial.println(currentState);
  switch (command) {
    case LEFT:
      turnLeft(pwm);
      break;
    case RIGHT:
      turnRight(pwm);
      break;
    case CIRCLE:

      break;
    case CROSS:
      forward(pwm);
      break;
    case TRIANGLE:
      backward(pwm);
      break;
    case SQUARE:
      digitalWrite(buzzerPin, HIGH);
      break;
    case START:
      // Perform action for starting a process or operation
      break;
    case PAUSE:
      // Perform action for pausing a process or operation
      break;
    case HALT:
      // Perform action for pausing a process or operation
      stop();
      break;
    default:
      // Invalid command received
      break;
  }
}

void tuning(char command) {
  switch (command) {
    case FORWARD:
      speedUp();
      break;
    case BACKWARD:
      speedDown();
      break;
    default:
      // Invalid command received
      break;
  }
}

void bunyi(int ms) {
  digitalWrite(buzzerPin, HIGH);
  delay(ms);
  digitalWrite(buzzerPin, LOW);
}