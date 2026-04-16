// --- Konfigurasi Pin ---
const int pinSensorPSI = A0;   // Pin Signal (In) sensor
const int pinLedMerah  = 5;
const int pinLedBiru   = 7;
const int pinLedHijau  = 6;
const int pinBuzzer    = 8;

// --- Konfigurasi Batas Tegangan (Volt) ---
const float voltMinimal = 1.5; // Ubah nilai ini sesuai batas PSI rendah (0.0 - 5.0 Volt)

void setup() {
  pinMode(pinLedMerah, OUTPUT);
  pinMode(pinLedBiru, OUTPUT);
  pinMode(pinLedHijau, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  
  Serial.begin(9600);

  // --- Beep Awal & Blue LED ---
  digitalWrite(pinLedBiru, HIGH);
  tone(pinBuzzer, 1000, 1000); 
  delay(1000);
  digitalWrite(pinLedBiru, LOW);
}

void loop() {
  // 1. Baca nilai mentah ADC (0-1023)
  int adcValue = analogRead(pinSensorPSI);
  
  // 2. Konversi nilai ADC ke Voltase menggunakan rumus:
  // Voltase = (Nilai_ADC / 1023.0) * 5.0
  float teganganSaatIni = adcValue * (5.0 / 1023.0);

  // 3. Bandingkan dengan Batas Tegangan
  if (teganganSaatIni < voltMinimal) {
    // KONDISI TEGANGAN RENDAH (ERROR)
    digitalWrite(pinLedMerah, HIGH);
    digitalWrite(pinLedHijau, LOW);
    
    // Beep putus-putus
    tone(pinBuzzer, 800);
    delay(150);
    noTone(pinBuzzer);
    delay(150);
  } else {
    // KONDISI NORMAL
    digitalWrite(pinLedHijau, HIGH);
    digitalWrite(pinLedMerah, LOW);
    noTone(pinBuzzer);
  }

  // Monitor di Serial
  Serial.print("Tegangan Sensor: ");
  Serial.print(teganganSaatIni);
  Serial.println(" V");
}