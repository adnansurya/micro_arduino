#include <SoftwareSerial.h>

// Definisikan pin SoftwareSerial untuk SIM800L
SoftwareSerial Serial1(10, 11); // RX, TX

void setup() {
  Serial.begin(9600);      // Serial ke komputer
  Serial1.begin(9600);     // Serial ke SIM800L via SoftwareSerial
  
  Serial.println("Testing SIM800L Connection...");
  delay(3000);
  Serial.println("READY");
}

void loop() {
  // Baca respon dari SIM800L
  if (Serial1.available()) {
    String response = Serial1.readString();
    Serial.print("SIM800L Response: ");
    Serial.println(response);
  }
  
  // Tampilkan data dari Serial Monitor ke SIM800L
  if(Serial.available() > 0){
    String comms = Serial.readStringUntil('\n');
    comms.trim();
    Serial.print("Perintah: ");
    Serial.println(comms);
    Serial1.println(comms);
    
    // Kirim Ctrl+Z (26) jika bukan perintah AT
    if(comms.indexOf("AT") < 0){
      Serial1.write(26);
    }
  }
  
  delay(2000);
}