// Konfigurasi pin dan konstanta
const int analogPin = 34;  // Pin analog untuk MQ135
const float RL = 10.0;     // Resistor load dalam kilo-ohm
const float VCC = 3.3;     // Tegangan catu daya ESP32
const float ratioCleanAir = 3.62; // Rasio RS/R0 di udara bersih

float R0 = 0; // Nilai referensi R0 (didapat dari kalibrasi)

void setup() {
  Serial.begin(115200);
  delay(2000); // Tunggu setup selesai

  // Kalibrasi sensor (pastikan di udara bersih)
  Serial.println("Kalibrasi sensor...");
  R0 = calibrateSensor();
  Serial.print("Nilai R0 (kalibrasi): ");
  Serial.println(R0);
}

void loop() {
  float rs = readSensor();
  float ratio = rs / R0;

  // Hitung PPM berdasarkan kurva datasheet
  float ppm = pow(10, ((log10(ratio) - (-0.45)) / (-0.32))); // m dan b diambil dari grafik datasheet MQ135 untuk CO

  Serial.print("RS: ");
  Serial.print(rs);
  Serial.print(" kOhm, PPM CO: ");
  Serial.println(ppm);
  
  delay(2000); // Delay 2 detik
}

float readSensor() {
  int adcValue = analogRead(analogPin);        // Baca nilai analog
  float sensorVoltage = (adcValue / 4095.0) * VCC; // Konversi ke tegangan (12-bit ADC)
  float rs = ((VCC - sensorVoltage) / sensorVoltage) * RL; // Hitung RS
  return rs;
}

float calibrateSensor() {
  float totalRS = 0;
  for (int i = 0; i < 100; i++) { // Ambil rata-rata dari 100 pembacaan
    totalRS += readSensor();
    delay(50);
  }
  float avgRS = totalRS / 100.0;
  return avgRS / ratioCleanAir; // Hitung R0 berdasarkan rasio udara bersih
}
