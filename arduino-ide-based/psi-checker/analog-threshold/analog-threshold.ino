// --- Konfigurasi Pin ---
const int pinSensorPSI = A0; 
const int pinLedMerah  = 5;
const int pinLedBiru   = 7;
const int pinLedHijau  = 6;
const int pinBuzzer    = 9;

// --- Konfigurasi Batas Tegangan (Volt) ---
const float voltMinimal = 2.5; 

void setup() {
  pinMode(pinLedMerah, OUTPUT);
  pinMode(pinLedBiru, OUTPUT);
  pinMode(pinLedHijau, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  
  Serial.begin(9600);

  // --- Beep Awal Cepat & Blue LED ---
  digitalWrite(pinLedBiru, HIGH);
  tone(pinBuzzer, 1500); 
  delay(150); 
  noTone(pinBuzzer);
  // digitalWrite(pinLedBiru, LOW);
  delay(2000);
}

void loop() {
  int adcValue = analogRead(pinSensorPSI);
  float teganganSaatIni = adcValue * (5.0 / 1023.0);

  if (teganganSaatIni < voltMinimal) {
    digitalWrite(pinLedMerah, HIGH);
    digitalWrite(pinLedHijau, LOW);
    
    tone(pinBuzzer, 800);
    delay(150);
    noTone(pinBuzzer);
    delay(150);
  } else {
    digitalWrite(pinLedHijau, HIGH);
    digitalWrite(pinLedMerah, LOW);
    noTone(pinBuzzer);
  }

  Serial.print("Tegangan Sensor: ");
  Serial.print(teganganSaatIni);
  Serial.println(" V");
}