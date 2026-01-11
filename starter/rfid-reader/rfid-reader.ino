#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN    4   // Pin SDA
#define RST_PIN   5  // Pin RST

MFRC522 mfrc522(SS_PIN, RST_PIN); // Inisialisasi MFRC522

void setup() {
  Serial.begin(115200);   // Inisialisasi komunikasi serial
  SPI.begin();            // Inisialisasi SPI bus
  mfrc522.PCD_Init();     // Inisialisasi MFRC522
  
  Serial.println("Dekatkan kartu RFID ke reader...");
  Serial.println();
}

void loop() {
  // Mengecek apakah ada kartu baru di depan reader
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Mengecek apakah data kartu bisa dibaca
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Menampilkan UID di Serial Monitor
  Serial.print("UID Tag :");
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  
  Serial.println();
  Serial.print("Pesan : ");
  content.toUpperCase();

  // Contoh Logika: Akses Diterima/Ditolak
  // Ganti "XX XX XX XX" dengan UID kartu Anda yang sudah terbaca nanti
  if (content.substring(1) == "83 23 8E 1A") { 
    Serial.println("Akses Diterima!");
    delay(3000);
  } else {
    Serial.println("Akses Ditolak");
    delay(3000);
  }
}