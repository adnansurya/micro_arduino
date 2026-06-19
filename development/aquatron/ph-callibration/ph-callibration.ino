// Definisi PIN ADC yang digunakan
const int potPin = 35; 

// Variabel untuk menampung nilai ADC dan pH
int adcValue = 0;
float nilaiPH = 0.0;

// Fungsi rumus pH hasil regresi linear dari 3 titik data Anda
float hitungPH(int adcRaw) {
  // Rumus: pH = (ADC * slope) + intercept
  float phHasil = (adcRaw * -0.00706) + 20.412;
  
  // Batasi output agar tetap berada di rentang pH logis (0 - 14)
  if (phHasil < 0.0) phHasil = 0.0;
  if (phHasil > 14.0) phHasil = 14.0;
  
  return phHasil;
}

void setup() {
  // Inisialisasi Serial Monitor
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=========================================");
  Serial.println("    ESP32 pH Meter (Calibrated Mode)     ");
  Serial.println("=========================================");
}

void loop() {
  // Multisampling (Ambil 10 sampel untuk kestabilan pembacaan)
  long sum = 0;
  int samples = 10;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(potPin);
    delay(10);
  }
  
  // Nilai rata-rata ADC
  adcValue = sum / samples;
  
  // Hitung nilai pH menggunakan fungsi rumus
  nilaiPH = hitungPH(adcValue);

  // Menampilkan hasil ke Serial Monitor (Tanpa Voltase)
  Serial.print("ADC Raw: ");
  Serial.print(adcValue);
  
  Serial.print(" | Nilai pH: ");
  Serial.println(nilaiPH, 2); // Menampilkan 2 angka di belakang koma

  delay(1000); // Update data setiap 1 detik
}