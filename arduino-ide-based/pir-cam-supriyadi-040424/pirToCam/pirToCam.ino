#include <BluetoothSerial.h>

#define pirPin 13
#define ledPin 2
#define relayPin 5

BluetoothSerial CameraBT;


int adaGerak = 0;
int lastGerak = 0;

void setup() {
  Serial.begin(115200);  // Open serial monitor at 115200 baud to see ping results.

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  delay(1000);
  digitalWrite(relayPin, LOW);

  pinMode(pirPin, INPUT);

  CameraBT.begin();  
  Serial.println("Bluetooth Started! Ready to pair...");

  kedipLed(2);
}

void loop() {

  if (CameraBT.available() > 0) {
    String dataCamera = CameraBT.readStringUntil('\n');
    dataCamera.trim();
    Serial.print("Dari Camera : ");
    Serial.println(dataCamera);
    kedipLed(0.5);
    if(dataCamera == "Buka Pintu"){
      digitalWrite(relayPin, HIGH);
    }else if(dataCamera == "Kunci Pintu"){
      digitalWrite(relayPin, LOW);
    }
    
  }

  adaGerak = digitalRead(pirPin);
  // Serial.print("ADA GERAK : ");
  // Serial.print(adaGerak);
  // Serial.println();

  if (adaGerak != lastGerak) {  //filter untuk mendeteksi perubahan status
    if (adaGerak) {
      Serial.println("Gerakan Terdeteksi");      
      CameraBT.print("Gerakan Terdeteksi");
      CameraBT.print("\n");
      kedipLed(3);    
    }
  }

  lastGerak = adaGerak;
  delay(50);
}

void kedipLed(float detik) {
  digitalWrite(ledPin, HIGH);
  delay(detik * 1000);
  digitalWrite(ledPin, LOW);
}
