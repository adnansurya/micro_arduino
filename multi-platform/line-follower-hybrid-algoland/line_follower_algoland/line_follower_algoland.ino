#include <SoftwareSerial.h>


bool manualMode = false;
int currentSpeed = 120;

// Pin untuk sensor infrared
int sensorKiri = 0;   // Pin untuk sensor inframerah kiri
int sensorKanan = 1;  // Pin untuk sensor inframerah kanan

unsigned long hitamBersamaStartTime = 0;
bool hitamBersamaDetected = false;
const long hitamDuration = 1000;  // Deteksi hitam bersama selama 1 detik untuk berhenti

bool berhentiPermanen = false;  // Untuk kondisi stop permanen nanti (opsional)
bool diGarisArray = false;      // Status apakah sedang di garis akhir

bool belokKiriSelanjutnya = true;  // Untuk mengatur belok kiri/kanan secara bergantian
// Menyimpan nilai sensor
int kiri = 0;
int kanan = 0;

// Nilai kalibrasi
int kiriMin = 1023, kiriMax = 0;
int kananMin = 1023, kananMax = 0;
int kiriThreshold = 512;
int kananThreshold = 512;

// Pin untuk motor driver
int IN1 = 7;  // Pin kontrol motor kiri (arah 1)
int IN2 = 8;  // Pin kontrol motor kiri (arah 2)
int ENA = 9;  // Pin PWM untuk kecepatan motor kiri
int IN3 = 3;  // Pin kontrol motor kanan (arah 1)
int IN4 = 4;  // Pin kontrol motor kanan (arah 2)
int ENB = 5;  // Pin PWM untuk kecepatan motor kanan

int pinRx = 10;
int pinTx = 11;

SoftwareSerial mySerial(pinRx, pinTx);

// Menyimpan nilai data motor driver
int PWMroda = 90;                         //80                    // Simpan kecepatan roda
float rodaKiri = 1.00, rodaKanan = 1.00;  // Meratakan tenaga motor

// Pin LED built-in
int ledPin = 13;

void setup() {
  Serial.begin(9600);
  delay(10);
  mySerial.begin(9600);

  SensorInfraredSetup();
  motorDriverSetup();
  pinMode(ledPin, OUTPUT);

  Serial.println("Mulai kalibrasi...");
  kalibrasiSensorOtomatis();
  Serial.println("Kalibrasi selesai.");

  Serial.println("Setup OK");
  delay(100);
}
void loop() {

  if (mySerial.available() > 0) {
    String perintah = mySerial.readStringUntil('\n');
    perintah.trim();
    Serial.print("Perintah: ");
    Serial.println(perintah);

    if (perintah == "mode:auto") {
      manualMode = false;
      Serial.println("MODE AUTO");
    }

    if (perintah == "mode:manual") {
      manualMode = true;
      Serial.println("MODE MANUAL");
    }


    if (manualMode) {
      if (perintah.indexOf("speed:") > -1) {

        String speedNow = perintah.substring(6);
        Serial.print("Speed Now: ");
        Serial.println(speedNow);
        currentSpeed = speedNow.toInt();


      } else {
        if (perintah == "arah:stop") {
          arah2Roda(0, 0);
          Serial.println("ARAH : STOP");
        }
        if (perintah == "arah:maju") {
          arah2Roda(1, currentSpeed);
          Serial.println("ARAH : MAJU");
        }
        if (perintah == "arah:mundur") {
          arah2Roda(2, currentSpeed);
          Serial.println("ARAH : MUNDUR");
        }
        if (perintah == "arah:kiri") {
          arah2Roda(3, currentSpeed);
          Serial.println("ARAH : KIRI");
        }
        if (perintah == "arah:kanan") {
          arah2Roda(4, currentSpeed);
          Serial.println("ARAH : KANAN");
        }
      }
    }
  }

  if (!manualMode) {

    SensorInfrared();

    int kiriStatus = (kiri > kiriThreshold) ? HIGH : LOW;
    int kananStatus = (kanan > kananThreshold) ? HIGH : LOW;

    // Cek apakah kedua sensor mendeteksi hitam (garis akhir atau persimpangan)
    if (kiriStatus == HIGH && kananStatus == HIGH) {
      static unsigned long startTimeHitam = millis();

      if (!diGarisArray) {
        startTimeHitam = millis();  // Reset timer tiap masuk garis
        diGarisArray = true;
      }

      if ((millis() - startTimeHitam) >= hitamDuration) {
        Serial.println("Berhenti di garis akhir.");
        arah2Roda(0, 0);
      } else {
        // Di titik persimpangan (+), belok bergantian kiri/kanan
        if (belokKiriSelanjutnya) {
          Serial.println("Belok kiri di persimpangan");
          arah2Roda(3, 120);  // Belok kiri
          belokKiriSelanjutnya = false;
        } else {
          Serial.println("Belok kanan di persimpangan");
          arah2Roda(4, 120);  // Belok kanan
          belokKiriSelanjutnya = true;
        }
      }

    } else {
      diGarisArray = false;

      // Jalankan logika line follower biasa
      if ((kiri < kiriThreshold) && (kanan < kananThreshold)) {
        arah2Roda(1, PWMroda);  // Maju
      } else if ((kiri > kiriThreshold) && (kanan < kananThreshold)) {
        arah2Roda(4, 120);  // Belok kanan
      } else if ((kiri < kiriThreshold) && (kanan > kananThreshold)) {
        arah2Roda(3, 120);  // Belok kiri
      }
    }

    Serial.print("Kiri: ");
    Serial.print(kiri);
    Serial.print(", Kanan: ");
    Serial.println(kanan);
    delay(5);
  }
  delay(1);
}
