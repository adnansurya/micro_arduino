#include <NewPing.h>
#include <Servo.h>

// Pin Ultrasonic
#define TRIG_PIN1 11
#define ECHO_PIN1 12
#define TRIG_PIN2 10
#define ECHO_PIN2 9
#define MAX_DISTANCE 200  // Jarak maksimal yang diukur dalam cm

// Servo
#define SERVO_PIN 6
Servo myServo;

// Ultrasonic sensor objects
NewPing sonar1(TRIG_PIN1, ECHO_PIN1, MAX_DISTANCE);
NewPing sonar2(TRIG_PIN2, ECHO_PIN2, MAX_DISTANCE);

// Variabel sudut servo
int targetAngle = 0;
int currentAngle = 0;

// Fungsi setup
void setup() {
  Serial.begin(9600);
  myServo.attach(SERVO_PIN);
  myServo.write(currentAngle);  // Set posisi awal servo ke 0
}

// Fungsi untuk menggerakkan servo secara perlahan
void moveServoSlowly(int target) {
  if (target > currentAngle) {
    for (int pos = currentAngle; pos <= target; pos++) {
      myServo.write(pos);
      delay(50);
    }
  } else {
    for (int pos = currentAngle; pos >= target; pos--) {
      myServo.write(pos);
      delay(50);
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
