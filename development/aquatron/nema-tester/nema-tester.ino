// Definisi pin ke driver A4988
const int stepPin = 3;
const int dirPin = 4;

// Jumlah langkah per putaran penuh untuk Nema 17 (Full Step = 200)
const int stepsPerRevolution = 200;

// Fungsi untuk mengonversi derajat ke jumlah langkah (steps)
int derajatKeStep(float derajat) {
  // Rumus: (Derajat / 360) * Total Step per Putaran
  return round((derajat / 360.0) * stepsPerRevolution);
}

// Fungsi untuk menggerakkan motor ke sudut spesifik dari posisi 0
void gerakKeSudut(float sudutTarget, int delaySpeed) {
  static float sudutSekarang = 0; // Menyimpan posisi sudut terakhir
  
  float selisihSudut = sudutTarget - sudutSekarang;
  int langkah = derajatKeStep(abs(selisihSudut));

  // Tentukan arah putaran
  if (selisihSudut > 0) {
    digitalWrite(dirPin, HIGH); // Searah jarum jam (CW)
  } else if (selisihSudut < 0) {
    digitalWrite(dirPin, LOW);  // Berlawanan arah jarum jam (CCW)
  }

  // Jalankan motor sebanyak langkah yang dihitung
  for (int i = 0; i < langkah; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(delaySpeed);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(delaySpeed);
  }

  sudutSekarang = sudutTarget; // Update posisi sudut sekarang
  delay(500); // Jeda singkat setiap selesai bergerak ke sudut target
}

void setup() {
  // Atur pin sebagai OUTPUT
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  
  // Kecepatan motor (makin kecil nilainya, makin cepat putarannya)
  int speedDelay = 1000; 

  // ==========================================
  // TAHAP 1: Sudut 90, 180, 270, 360 (2 Kali)
  // ==========================================
  for (int putaran = 0; putaran < 2; putaran++) {
    gerakKeSudut(90, speedDelay);
    gerakKeSudut(180, speedDelay);
    gerakKeSudut(270, speedDelay);
    gerakKeSudut(360, speedDelay);
    
    // Reset sudut ke 0 secara virtual untuk putaran kedua
    // agar motor mendeteksi posisi 360 sama dengan posisi 0 awal.
    if(putaran == 0) {
      // Kita langsung set acuan internal fungsi menjadi 0 tanpa menggerakkan motor
      // Karena posisi 360 derajat secara fisik sama dengan 0 derajat.
      extern float sudutSekarang; 
      // Karena menggunakan static di dalam fungsi, alternatifnya kita buat rotasi kumulatif:
    }
  }
  
  // Untuk mempermudah logika sekuensial berikutnya, kita reset motor ke posisi awal fisik
  // atau kita gunakan pergerakan relatif/akumulatif ke depan.
  
  // Mari kita gunakan pendekatan akumulatif total agar motor terus berputar maju:
  float totalSudut = 0;

  // Reset pergerakan (opsional, jeda sebelum masuk tahap berikutnya)
  delay(2000);

  // ==========================================
  // TAHAP 2: Kelipatan 30 derajat (2 Putaran penuh)
  // 2 Putaran = 720 derajat. Kelipatan 30 artinya ada 24 pemberhentian.
  // ==========================================
  for (int i = 0; i < 2; i++) { // 2 kali putaran
    for (int sudut = 30; sudut <= 360; sudut += 30) {
      totalSudut += 30;
      gerakKeSudut(totalSudut, speedDelay);
    }
  }

  delay(2000); // Jeda antar tahap

  // ==========================================
  // TAHAP 3: Kelipatan 10 derajat (2 Putaran penuh)
  // 2 Putaran = 720 derajat. Kelipatan 10 artinya ada 72 pemberhentian.
  // ==========================================
  for (int i = 0; i < 2; i++) { // 2 kali putaran
    for (int sudut = 10; sudut <= 360; sudut += 10) {
      totalSudut += 10;
      gerakKeSudut(totalSudut, speedDelay);
    }
  }
}

void loop() {
  // Loop dikosongkan karena semua skenario diminta berjalan berurutan lalu selesai.
  // Jika ingin mengulang dari awal, pindahkan semua kode di dalam setup() ke sini.
}