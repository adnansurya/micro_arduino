#include <Servo.h>

// Pin definisi
const int trigPin = 9;
const int echoPin = 8;
const int servoPin = 3;

const int sudutBuka = 160;
const int sudutTutup = 50;

// Objek servo
Servo tutupSampah;

// Jarak minimal untuk membuka tutup (dalam cm)
const int batasJarak = 20;

// Variabel waktu dan jarak
long durasi;
int jarak;

void setup() {
  // Inisialisasi pin
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Mulai komunikasi serial (opsional untuk debugging)
  Serial.begin(9600);

  // Attach servo ke pin
  tutupSampah.attach(servoPin);
  tutupSampah.write(sudutTutup); // Tutup awal
}

void loop() {
  // Kirim sinyal ultrasonik
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Hitung waktu pantulan
  durasi = pulseIn(echoPin, HIGH);

  // Hitung jarak (dalam cm)
  jarak = durasi * 0.034 / 2;

  // Tampilkan di Serial Monitor
  Serial.print("Jarak: ");
  Serial.print(jarak);
  Serial.println(" cm");

  // Logika buka/tutup tutup tempat sampah
  if (jarak > 0 && jarak <= batasJarak) {
    tutupSampah.write(sudutBuka); // Buka
    Serial.print("BUKA");
    delay(2000);           // Tunggu 2 detik
  } else {
    tutupSampah.write(sudutTutup);  // Tutup
    Serial.print("TUTUP");
  }

  delay(200); // Delay loop
}
