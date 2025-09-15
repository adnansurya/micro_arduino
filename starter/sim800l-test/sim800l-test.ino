void setup() {
  Serial.begin(9600);      // Serial ke komputer
  Serial1.begin(9600);     // Serial ke SIM800L (RX1=19, TX1=18)
  
  Serial.println("Testing SIM800L Connection...");
  delay(3000);
  Serial.println("READY");
}

void loop() {
  // Kirim AT command ke SIM800L
  // Serial1.println("AT");
  // delay(1000);
  
  // Baca respon dari SIM800L
  if (Serial1.available()) {
    String response = Serial1.readString();
    Serial.print("SIM800L Response: ");
    Serial.println(response);
  }
  
  // Tampilkan data dari Serial Monitor ke SIM800L
  // if (Serial.available()) {
  //   char c = Serial.read();
  //   Serial1.write(c);
  // }

  if(Serial.available() > 0){
    String comms = Serial.readStringUntil('\n');
    comms.trim();
    Serial.print("Perintah: ");
    Serial.println(comms);
    Serial1.println(comms);
    if(comms.indexOf("AT") < 0){
      Serial1.write(26);
    }
  }
  
  delay(2000);
}