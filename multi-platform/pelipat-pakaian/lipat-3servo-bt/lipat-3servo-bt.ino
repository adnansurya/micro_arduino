#include <Servo.h>

#define PIN_KANAN 9
#define PIN_ATAS 10
#define PIN_KIRI 11

#define idleKanan 180
#define idleAtas 0
#define idleKiri 0


#define lipatKanan 130
#define lipatAtas 90
#define lipatKiri 150

Servo servoKanan, servoAtas, servoKiri;  // create Servo object to control a servo


void setup() {
  // put your setup code here, to run once:
  servoKanan.attach(PIN_KANAN);
  servoKiri.attach(PIN_KIRI);
  servoAtas.attach(PIN_ATAS);

  servoKanan.write(idleKanan);
  servoAtas.write(idleAtas);
  servoKiri.write(idleKiri);

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
  servoKanan.write(lipatKanan);
  delay(delayLipat);
  servoKanan.write(idleKanan);
  delay(delayLipat);
  servoKiri.write(lipatKiri);
  delay(delayLipat);
  servoKiri.write(idleKiri);
  delay(delayLipat);
  servoAtas.write(lipatAtas);
  delay(delayLipat);
  servoAtas.write(idleAtas);
  delay(delayLipat);

  Serial.println("Selesai");
}
