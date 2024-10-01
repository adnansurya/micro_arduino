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


#define enA 5
#define in1 6
#define in2 7
#define in3 8
#define in4 9
#define enB 10



SoftwareSerial mySerial(12, 11);  // RX, TX

int motorSpeed = 100;
String currentState = "stop";
char currentChar = ' ';
char lastChar = ' ';
int actionVal = 25;


void setup() {
  // Set all the motor control pins to outputs
  pinMode(enA, OUTPUT);
  pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  // Turn off motors - Initial state
  stop();

  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  mySerial.begin(9800);
}

void loop() {
  if (mySerial.available()) {
    char command = mySerial.read();
    currentChar = command;
    Serial.print("CHAR : ");
    Serial.println(currentChar);

    if (currentChar != lastChar && currentChar != 0) {
      executeCommand(currentChar, motorSpeed);     
      Serial.println(currentState); 
    }

    lastChar = currentChar;

    // delay(1000);
  }
}




void stop() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
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
  analogWrite(enB, pwm - 20);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  currentState = "turnLeft : " + String(pwm);
}

void turnRight(int pwm) {
  analogWrite(enA, pwm - 20);
  analogWrite(enB, pwm);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
  currentState = "turnRight : " + String(pwm);
}

void speedDown() {
  motorSpeed = motorSpeed - actionVal;
  Serial.print("Speed Down : ");
  Serial.println(motorSpeed);
  executeCommand(currentChar, motorSpeed);
}

void speedUp() {
  motorSpeed = motorSpeed + actionVal;
  Serial.print("Speed Up : ");
  Serial.println(motorSpeed);
  executeCommand(currentChar, motorSpeed);
}

void executeCommand(char command, int pwm) {
  switch (command) {
    case FORWARD:
      speedUp();
      break;
    case BACKWARD:
      speedDown();
      break;
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
      stop();
      break;
    case START:
      // Perform action for starting a process or operation
      break;
    case PAUSE:
      // Perform action for pausing a process or operation
      break;
    default:
      // Invalid command received
      break;
  }
}