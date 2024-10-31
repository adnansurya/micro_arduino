// ---------------------------------------------------------------------------
// Example NewPing library sketch that pings 3 sensors 20 times a second.
// ---------------------------------------------------------------------------

#include <NewPing.h>

#include <Servo.h>

Servo servo1, servo2;  // create servo object to control a servo

#define SONAR_NUM 2       // Number of sensors.
#define MAX_DISTANCE 200  // Maximum distance (in cm) to ping.

#define batas 10
#define toleransiSelisih 5

int sudut[3] = { 0, 120, 240 };

NewPing sonar[SONAR_NUM] = {
  // Sensor object array.
  NewPing(2, 3, MAX_DISTANCE),
  NewPing(4, 5, MAX_DISTANCE)  // Each sensor's trigger pin, echo pin, and max distance to ping.

};

void setup() {
  Serial.begin(9600);  // Open serial monitor at 115200 baud to see ping results.

  servo1.attach(9);
  // servo2.attach(10);


  servo1.write(90);
  // servo2.write(90);

  delay(1000);
  servo1.write(0);
  // servo2.write(0);
}

void loop() {
  int jarak1, jarak2;



  jarak1 = sonar[0].ping_cm();
  Serial.print("JARAK_1: ");
  Serial.print(jarak1);


  jarak2 = sonar[1].ping_cm();
  Serial.print("\nJARAK_2: ");
  Serial.println(jarak2);

  // if ((jarak1 < 60 && jarak2 < 60)) {
  if (jarak1 < jarak2  && (jarak2-jarak1) > toleransiSelisih ) {
    Serial.println("KANAN");
    servo1.write(120);
    delay(500);
  } else if (jarak2 < jarak1 && (jarak1-jarak2) > toleransiSelisih) {
    Serial.println("KIRI");
    servo1.write(0);
    delay(500);
  } else if (abs(jarak1 - jarak2) < toleransiSelisih) {
    Serial.println("RANDOM");
    servo1.write(random(180));
    delay(500);
  }
  // }else{

  // }


  // Serial.println();
  delay(100);
}