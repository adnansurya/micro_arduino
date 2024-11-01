#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>

// Pin Ultrasonic
#define TRIG_PIN1 7
#define ECHO_PIN1 6
#define TRIG_PIN2 5
#define ECHO_PIN2 4
#define MAX_DISTANCE 200  // Jarak maksimal yang diukur dalam cm

// Servo
#define SERVO_CHANNEL 0 // Servo pada kanal 0 di PCA9685
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Konfigurasi Servo
#define SERVO_MIN 150    // Pulse width minimum (0°)
#define SERVO_MAX 600    // Pulse width maximum (180°)

// Ultrasonic sensor objects
NewPing sonar1(TRIG_PIN1, ECHO_PIN1, MAX_DISTANCE);
NewPing sonar2(TRIG_PIN2, ECHO_PIN2, MAX_DISTANCE);

// Variabel sudut servo
int targetAngle = 0;
int currentAngle = 0;

// Fungsi untuk mengatur sudut servo pada PCA9685
void setServoAngle(int angle) {
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(SERVO_CHANNEL, 0, pulse);
}

// Fungsi setup
void setup() {
  Serial.begin(9600);
  pwm.begin();
  pwm.setPWMFreq(60);  // PCA9685 frekuensi untuk servo (biasanya 60 Hz)

  // Set posisi awal servo ke sudut 0
  setServoAngle(currentAngle);
  delay(500);
}

// Fungsi untuk menggerakkan servo secara perlahan
void moveServoSlowly(int target) {
  if (target > currentAngle) {
    for (int pos = currentAngle; pos <= target; pos++) {
      setServoAngle(pos);
      delay(80);
    }
  } else {
    for (int pos = currentAngle; pos >= target; pos--) {
      setServoAngle(pos);
      delay(80);
    }
  }
  currentAngle = target;  // Update posisi sudut servo saat ini
}

// Fungsi loop utama
void loop() {
  // Membaca jarak dari kedua sensor ultrasonic
  int distance1 = sonar1.ping_cm();
  int distance2 = sonar2.ping_cm();

  // Cetak jarak ke Serial Monitor
  Serial.print("Distance 1: ");
  Serial.print(distance1);
  Serial.print(" cm, ");
  Serial.print("Distance 2: ");
  Serial.print(distance2);
  Serial.println(" cm");

  // Membandingkan jarak dari kedua sensor
  if (distance1 > 0 && distance2 > 0) {  // Pastikan pembacaan valid
    if (distance1 < distance2) {  // Sensor 1 lebih dekat
      targetAngle = 0;
    } else {  // Sensor 2 lebih dekat
      targetAngle = 180;
    }

    // Menggerakkan servo ke target angle secara perlahan
    if (targetAngle != currentAngle) {
      moveServoSlowly(targetAngle);
    }
  }
  
  delay(200);  // Tambahan delay untuk stabilitas pembacaan sensor
}
