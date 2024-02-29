#include <Servo.h>
#include <SoftwareSerial.h>

#define servo1Pin 5
#define servo2Pin 6

const int delayServo = 2000;

Servo servo1, servo2;
SoftwareSerial mySerial(10, 11);  // RX, TX

String dataTerima = "";
String kategori = "";

bool diproses = false;

void setup() {

  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  mySerial.begin(9600);

  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);

  servo1.write(90);
  servo2.write(90);

  Serial.println("READY");
}

void loop() {

  if (mySerial.available() > 0) {
    dataTerima = mySerial.readStringUntil('\n');
    dataTerima.trim();
    Serial.print("DATA TERIMA : ");
    Serial.println(dataTerima);

    if (dataTerima.indexOf("kategori:") > -1) {

      kategori = dataTerima.substring(9);
      Serial.print("Kategori : ");
      Serial.println(kategori);

      diproses = true;
    }
  }

  if (diproses) {
    mySerial.println("DIPROSES");

    if(kategori == "unrecognized"){
      sampahTakDikenal();
    }else if(kategori == "plastic"){
      sampahPlastik();
    }else if(kategori == "metal"){
      sampahMetal();
    }
    delay(1000);

    mySerial.println("STOP");
    diproses = false;
  }
}

void sampahTakDikenal() {
  servo1.write(0);
  delay(delayServo);
  servo1.write(90);  
}

void sampahMetal(){
  servo1.write(180);
  delay(delayServo);
  

  servo2.write(0);
  delay(delayServo);
  servo2.write(90);
  servo1.write(90);
}

void sampahPlastik(){
  servo1.write(180);
  delay(delayServo);  

  servo2.write(180);
  delay(delayServo);
  servo2.write(90);
  servo1.write(90);
}
