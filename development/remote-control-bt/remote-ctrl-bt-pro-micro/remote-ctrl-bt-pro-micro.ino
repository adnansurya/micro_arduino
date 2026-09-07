#include <SoftwareSerial.h>

// Deklarasi pin Bluetooth (RX, TX)
// Pin 10 Arduino terhubung ke TX Bluetooth, Pin 11 Arduino terhubung ke RX Bluetooth
SoftwareSerial btSerial(10, 11); 

// Deklarasi pin Motor Driver
const int motor1A = 2; // Motor Kiri Maju
const int motor1B = 3; // Motor Kiri Mundur
const int motor2A = 4; // Motor Kanan Maju
const int motor2B = 5; // Motor Kanan Mundur

// Deklarasi pin Klakson
const int buzzerPin = 8;

char command;

void setup() {
  // Inisialisasi komunikasi Serial Bluetooth
  btSerial.begin(9600);
  
  // Set pin motor sebagai OUTPUT
  pinMode(motor1A, OUTPUT);
  pinMode(motor1B, OUTPUT);
  pinMode(motor2A, OUTPUT);
  pinMode(motor2B, OUTPUT);
  
  // Set pin buzzer sebagai OUTPUT
  pinMode(buzzerPin, OUTPUT);

  // Pastikan kondisi awal semua mati
  berhenti();
}

void loop() {
  // Cek jika ada data masuk dari Bluetooth
  if (btSerial.available() > 0) {
    command = btSerial.read();

    // Kontrol Arah Pergerakan
    switch (command) {
      case 'F': // Forward (Maju)
        maju();
        break;
      case 'B': // Back (Mundur)
        mundur();
        break;
      case 'L': // Left (Belok Kiri)
        kiri();
        break;
      case 'R': // Right (Belok Kanan)
        kanan();
        break;
      case 'S': // Stop (Berhenti)
        berhenti();
        break;

      // Kontrol Klakson
      case 'T': // Horn ON (Klakson Bunyi)
        digitalWrite(buzzerPin, HIGH);
        delay(100);
        digitalWrite(buzzerPin, LOW);
        
      
    }
  }
}

// Fungsi Gerak Maju
void maju() {
  digitalWrite(motor1A, HIGH);
  digitalWrite(motor1B, LOW);
  digitalWrite(motor2A, HIGH);
  digitalWrite(motor2B, LOW);
}

// Fungsi Gerak Mundur
void mundur() {
  digitalWrite(motor1A, LOW);
  digitalWrite(motor1B, HIGH);
  digitalWrite(motor2A, LOW);
  digitalWrite(motor2B, HIGH);
}

// Fungsi Belok Kiri
void kiri() {
  digitalWrite(motor1A, LOW);
  digitalWrite(motor1B, HIGH);
  digitalWrite(motor2A, HIGH);
  digitalWrite(motor2B, LOW);
}

// Fungsi Belok Kanan
void kanan() {
  digitalWrite(motor1A, HIGH);
  digitalWrite(motor1B, LOW);
  digitalWrite(motor2A, LOW);
  digitalWrite(motor2B, HIGH);
}

// Fungsi Berhenti
void berhenti() {
  digitalWrite(motor1A, LOW);
  digitalWrite(motor1B, LOW);
  digitalWrite(motor2A, LOW);
  digitalWrite(motor2B, LOW);
}