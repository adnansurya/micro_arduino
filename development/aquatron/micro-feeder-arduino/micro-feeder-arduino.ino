#include <Stepper.h> 

const int stepsPerRevolution = 200;
Stepper myStepper = Stepper(stepsPerRevolution, 11, 10, 9, 8);

// Menggunakan pin Analog A0
const int pinAnalog = A0; 

// Batas Nilai ADC (Silakan sesuaikan dengan kebutuhan Anda)
// Contoh: Motor akan aktif jika nilai ADC di bawah 300
const int batasADC = 300; 

void setup() {
  myStepper.setSpeed(100); 
  
  // Mengaktifkan komunikasi Serial ke komputer
  Serial.begin(9600); 
  
  matikanArusMotor();
}

void loop() {
  // Membaca nilai ADC dari pin A0 (hasilnya antara 0 - 1023)
  int nilaiADC = analogRead(pinAnalog);
  
  // Menampilkan nilai ADC ke Serial Monitor
  Serial.print("Nilai ADC saat ini: ");
  Serial.println(nilaiADC);
  
  // Cek apakah nilai ADC memenuhi syarat (berada di bawah batas)
  if (nilaiADC < batasADC) {
    // Memutar 1 langkah kecil searah jarum jam
    myStepper.step(1); 
  } else {
    // Jika di atas batas, motor langsung mati agar L298N tetap dingin
    matikanArusMotor();
  }
  
  // Jeda sangat singkat agar tampilan Serial Monitor tidak terlalu cepat 
  // dan pembacaan sensor lebih stabil
  delay(10); 
}

// Fungsi menjaga agar L298N tetap dingin saat motor diam
void matikanArusMotor() {
  digitalWrite(11, LOW);
  digitalWrite(10, LOW);
  digitalWrite(9, LOW);
  digitalWrite(8, LOW);
}