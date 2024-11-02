#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <NewPing.h>

// Konfigurasi Pin Ultrasonic
#define NUM_SENSORS 7
const int trigPins[NUM_SENSORS] = { 2, 4, 6, 8, 10, 30, 32 };  // Trig pins untuk masing-masing sensor
const int echoPins[NUM_SENSORS] = { 3, 5, 7, 9, 11, 31, 33 };  // Echo pins untuk masing-masing sensor
#define MAX_DISTANCE 200                                       // Jarak maksimal yang diukur dalam cm

// Servo PCA9685
#define SERVO_MIN 150  // Pulse width minimum (0°)
#define SERVO_MAX 600  // Pulse width maximum (180°)
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

int delaySpeed = 30;

// Objek sensor ultrasonic
NewPing sonar[NUM_SENSORS] = {
  NewPing(trigPins[0], echoPins[0], MAX_DISTANCE),
  NewPing(trigPins[1], echoPins[1], MAX_DISTANCE),
  NewPing(trigPins[2], echoPins[2], MAX_DISTANCE),
  NewPing(trigPins[3], echoPins[3], MAX_DISTANCE),
  NewPing(trigPins[4], echoPins[4], MAX_DISTANCE),
  NewPing(trigPins[5], echoPins[5], MAX_DISTANCE),
  NewPing(trigPins[6], echoPins[6], MAX_DISTANCE)
};

// Set sudut target untuk setiap sensor (7 set sudut untuk setiap kemungkinan hasil)
int targetAnglesSet[NUM_SENSORS][NUM_SENSORS] = {
  { 90, 45, 30, 45, 70, 75, 50 },
  { 135, 90, 50, 80, 75, 65, 45 },
  { 145, 135, 90, 85, 80, 55, 40 },
  { 135, 130, 95, 90, 85, 50, 45 },
  { 140, 125, 100, 95, 90, 45, 35 },
  { 135, 115, 105, 100, 130, 90, 45 },
  { 130, 105, 110, 120, 135, 135, 90 }
};

// Array untuk menyimpan sudut saat ini dari setiap servo
int currentAngles[NUM_SENSORS] = { 0, 0, 0, 0, 0, 0, 0 };  // Semua servo mulai dari sudut 0 derajat

// Fungsi untuk mengatur sudut servo pada PCA9685
void setServoAngle(int servoChannel, int angle) {
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(servoChannel, 0, pulse);
}

// Fungsi untuk menggerakkan servo secara perlahan ke target angle
void moveServoSlowly(int servoChannel, int targetAngle) {
  int currentAngle = currentAngles[servoChannel];

  if (targetAngle > currentAngle) {
    for (int pos = currentAngle; pos <= targetAngle; pos++) {
      setServoAngle(servoChannel, pos);
      delay(delaySpeed);
    }
  } else {
    for (int pos = currentAngle; pos >= targetAngle; pos--) {
      setServoAngle(servoChannel, pos);
      delay(delaySpeed);
    }
  }

  // Update current angle untuk servo ini
  currentAngles[servoChannel] = targetAngle;
}

// Fungsi setup
void setup() {
  Serial.begin(9600);
  pwm.begin();
  pwm.setPWMFreq(60);  // PCA9685 frekuensi untuk servo (biasanya 60 Hz)

  // Set posisi awal servo ke sudut default
  for (int i = 0; i < NUM_SENSORS; i++) {
    setServoAngle(i, 0);  // Semua servo mulai dari sudut 0 derajat
  }
  delay(1000);
}

// Fungsi loop utama
void loop() {
  int minDistance = MAX_DISTANCE;
  int closestSensor = -1;

  // Membaca jarak dari setiap sensor ultrasonic
  for (int i = 0; i < NUM_SENSORS; i++) {
    int distance = sonar[i].ping_cm();
    if (distance <= 0) {
      distance = MAX_DISTANCE;
    }
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(distance);
    Serial.println(" cm");

    // Memeriksa apakah jarak saat ini lebih dekat dari jarak minimum yang ditemukan
    if (distance > 0 && distance < minDistance) {  // Mengabaikan pembacaan 0 (error)
      minDistance = distance;
      closestSensor = i;  // Menyimpan indeks sensor dengan jarak terpendek
    }
    delay(10);
  }

  // Jika ada sensor yang terdeteksi sebagai yang terdekat, gerakkan semua servo ke sudut yang sesuai
  if (closestSensor != -1) {
    Serial.print("Closest sensor: ");
    Serial.println(closestSensor + 1);

    // Gerakkan semua servo sesuai set sudut target berdasarkan sensor terdekat
    for (int i = 0; i < NUM_SENSORS; i++) {
      int targetAngle = targetAnglesSet[closestSensor][i];
      Serial.print("Moving servo ");
      Serial.print(i + 1);
      Serial.print(" to angle ");
      Serial.println(targetAngle);
      moveServoSlowly(i, targetAngle);  // Gerakkan servo secara perlahan
    }
  }

  delay(500);  // Tambahan delay untuk stabilitas pembacaan sensor
}
