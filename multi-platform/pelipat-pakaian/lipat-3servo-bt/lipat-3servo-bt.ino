#include <Servo.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define PIN_KANAN 9
#define PIN_ATAS 10
#define PIN_KIRI 11

#define idleKanan 180
#define idleAtas 30
#define idleKiri 0


#define lipatKanan 90
#define lipatAtas 110
#define lipatKiri 90

Servo servoKanan, servoAtas, servoKiri;  // create Servo object to control a servo

// Servo PCA9685
#define SERVO_MIN 150  // Pulse width minimum (0°)
#define SERVO_MAX 600  // Pulse width maximum (180°)
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

void setup() {

  pwm.begin();
  pwm.setPWMFreq(60);
  // put your setup code here, to run once:
  // servoKanan.attach(PIN_KANAN);
  // servoKiri.attach(PIN_KIRI);
  // servoAtas.attach(PIN_ATAS);

  // servoKanan.write(idleKanan);
  // servoAtas.write(idleAtas);
  // servoKiri.write(idleKiri);

  setServoAngle(0, idleKanan);
  setServoAngle(1, idleAtas);
  setServoAngle(2, idleKiri);
  

  Serial.begin(9600);
  Serial.println("Ready");
}

void loop() {
  if (Serial.available() > 0) {
    String perintah = Serial.readStringUntil('\n');
    perintah.trim();
    if (perintah == "lipat") {
      lipat(1000);      
    }
  }
  delay(10);
}



void lipat(int delayLipat) {
  Serial.println("Mulai");
  setServoAngle(0, lipatKanan);
  delay(delayLipat);
  setServoAngle(0, idleKanan);
  delay(delayLipat);
  setServoAngle(2, lipatKiri);
  delay(delayLipat);
  setServoAngle(2, idleKiri);
  delay(delayLipat);
  setServoAngle(1, lipatAtas);
  delay(delayLipat);
  setServoAngle(1, idleAtas);
  delay(delayLipat);

  Serial.println("Selesai");
}

void setServoAngle(int servoChannel, int angle) {
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(servoChannel, 0, pulse);
}
