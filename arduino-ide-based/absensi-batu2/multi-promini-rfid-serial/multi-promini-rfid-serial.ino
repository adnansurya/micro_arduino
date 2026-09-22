#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

#define SS_PIN 10
#define RST_PIN 9
#define BUZZER_PIN 8  // Pin Buzzer diletakkan pada Pin 8

// Pin SoftwareSerial komunikasi ke ESP32
// Pin 2 sebagai RX, Pin 3 sebagai TX
SoftwareSerial espSerial(2, 3);

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup() {
  // Serial Hardware (Pin 0/1) untuk Debugging ke USB-to-TTL FTDI (PC)
  Serial.begin(115200); 
  
  // SoftwareSerial untuk kirim data ke ESP32
  espSerial.begin(9600); 

  // Inisialisasi Pin Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Pastikan buzzer mati saat awal mula

  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("Arduino Pro Mini RFID Reader Ready (SoftwareSerial)...");
}

void loop() {
  // Cek apakah ada kartu RFID baru
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Format UID kartu ke String HEX
  String uidStr = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidStr += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    uidStr += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();

  // Trigger Beep Buzzer sekejap (100 milisekon) saat kartu terdeteksi
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);

  // Kirim UID ke ESP32 melalui SoftwareSerial diakhiri newline '\n'
  espSerial.println("RFID:" + uidStr);

  // Tampilkan log debug pada Serial Monitor PC (FTDI)
  Serial.print("Kartu Terbaca & Terkirim ke ESP32: ");
  Serial.println(uidStr);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(1000); // Delay untuk mencegah pembacaan ganda berulang
}