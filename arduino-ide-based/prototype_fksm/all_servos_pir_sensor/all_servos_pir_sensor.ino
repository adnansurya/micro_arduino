#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Inisialisasi modul PCA9685 untuk mengontrol servo
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Konfigurasi pin sensor gerak PIR
const int pirPin = 9;

// Variabel kontrol waktu
unsigned long lastMotionTime = 0;
const unsigned long motionTimeout = 10000;  // 5 detik


// Konfigurasi sudut servo
int minServoPos = 150;     // Sudut minimum (kanan)
int centerServoPos = 375;  // Posisi tengah (depan)
int maxServoPos = 600;     // Sudut maksimum (kiri)
const int numServos = 7;   // Jumlah servo

// Array untuk menyimpan posisi terakhir setiap servo
int servoPositions[numServos];

// Status deteksi gerakan dan perintah serial
bool motionDetected = false;
bool serialCommandDetected = false;

void setup() {
  Serial.begin(9600);
  pinMode(pirPin, INPUT);

  // Inisialisasi PCA9685
  pwm.begin();
  pwm.setPWMFreq(60);  // Frekuensi untuk servo
  // delay(5000);

  Serial.println("Program dimulai");
  moveAllServos(minServoPos);
 
}

void loop() {
  // Cek deteksi gerakan melalui sensor PIR
  motionDetected = digitalRead(pirPin);

  // Cek input dari komunikasi serial
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    // Deteksi perintah berdasarkan teks input
    if (command.equalsIgnoreCase("gerak")) {
      serialCommandDetected = true;
      lastMotionTime = millis();  // Perbarui waktu deteksi
      Serial.println("Perintah 'gerak' diterima dari serial");
    } else if (command == "kanan" || command == "reset") {
      moveAllServos(minServoPos);  // Semua servo ke posisi kanan
      Serial.println("Semua servo bergerak ke kanan");
    } else if (command == "depan") {
      moveAllServos(centerServoPos);  // Semua servo ke posisi depan
      Serial.println("Semua servo bergerak ke depan");
    } else if (command == "kiri") {
      moveAllServos(maxServoPos);  // Semua servo ke posisi kiri
      Serial.println("Semua servo bergerak ke kiri");
    }
  }

  // Jika terdeteksi gerakan atau menerima perintah 'gerak' melalui serial
  if (motionDetected || serialCommandDetected) {
    if (motionDetected) {
      Serial.println("Gerakan Terdeteksi");
    }
    serialCommandDetected = false;
    lastMotionTime = millis();

    // Gerakkan servo satu per satu ke posisi acak secara perlahan
    for (int i = 0; i < numServos; i++) {
      int randomPos = random(minServoPos, maxServoPos);
      moveServoSlowly(i, servoPositions[i], randomPos);
      servoPositions[i] = randomPos;  // Simpan posisi akhir servo
      // delay(500); // Jeda antar servo
    }
  } else {
    // Jika tidak ada gerakan atau perintah 'gerak' selama waktu yang ditentukan, hentikan servo
    // if (millis() - lastMotionTime > motionTimeout) {
    //   stopAllServos();
    // }
  }
}


void moveServoSlowly(int servoNum, int startPos, int targetPos) {
  int step = (targetPos > startPos) ? 1 : -1;

  Serial.print("Moving servo ");
  Serial.print(servoNum);
  Serial.print(" to angle ");
  int targetAngleConv = map(targetPos, minServoPos, maxServoPos, 0, 180);
  Serial.println(targetAngleConv);

  // Gerakkan servo dari posisi awal ke posisi target secara perlahan
  for (int pos = startPos; pos != targetPos; pos += step) {
    pwm.setPWM(servoNum, 0, pos);
    // Serial.print("move servo ");
    // Serial.print(servoNum);
    // Serial.print(" to ");
    // Serial.print(pos);
    delay(10);  // Kecepatan pergerakan tiap servo
  }
}

void moveAllServos(int targetPos) {
  // Gerakkan semua servo ke posisi target yang sama
  for (int i = 0; i < numServos; i++) {
    moveServoSlowly(i, servoPositions[i], targetPos); // Mulai dari posisi terakhir servo
    servoPositions[i] = targetPos; // Simpan posisi target sebagai posisi terakhir
  }
}

void stopAllServos() {
  for (int i = 0; i < numServos; i++) {
    pwm.setPWM(i, 0, 0);  // Hentikan servo
  }
  Serial.println("Semua servo berhenti.");
}
