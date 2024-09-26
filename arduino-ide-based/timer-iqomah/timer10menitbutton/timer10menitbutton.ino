const int buttonPin = 2;   // Pin tombol
unsigned long startMillis; // Variabel untuk menyimpan waktu mulai
unsigned long currentMillis; // Variabel untuk menyimpan waktu saat ini
const unsigned long period = 600000; // 10 menit dalam milidetik (600000 ms = 10 menit)
bool timerRunning = false;  // Status apakah timer sedang berjalan
bool timerFinished = false; // Status apakah timer sudah selesai

void setup() {
  pinMode(buttonPin, INPUT_PULLUP); // Mengatur pin tombol dengan pull-up internal
  Serial.begin(9600);  // Memulai komunikasi serial
  Serial.println("Standby, tekan tombol untuk memulai timer.");
}

void loop() {
  // Membaca status tombol
  if (digitalRead(buttonPin) == LOW) { // Tombol ditekan (LOW karena pull-up)
    if (!timerRunning) {
      startMillis = millis();  // Simpan waktu mulai
      timerRunning = true;     // Mengaktifkan timer
      timerFinished = false;   // Reset status selesai
      Serial.println("Timer dimulai!");
      delay(200);  // Debouncing tombol
    }
  }

  // Jika timer sedang berjalan
  if (timerRunning) {
    currentMillis = millis(); // Dapatkan waktu saat ini

    if (currentMillis - startMillis < period) {
      unsigned long remainingTime = period - (currentMillis - startMillis); // Waktu tersisa dalam milidetik
      unsigned int minutes = remainingTime / 60000;  // Konversi ke menit
      unsigned int seconds = (remainingTime % 60000) / 1000;  // Konversi ke detik

      // Tampilkan waktu dalam format mm:ss
      Serial.print("Sisa waktu: ");
      if (minutes < 10) {
        Serial.print('0');  // Tambahkan 0 jika menit kurang dari 10
      }
      Serial.print(minutes);
      Serial.print(':');
      if (seconds < 10) {
        Serial.print('0');  // Tambahkan 0 jika detik kurang dari 10
      }
      Serial.println(seconds);

      delay(1000);  // Tunggu 1 detik sebelum memperbarui waktu
    } else {
      Serial.println("Timer selesai!");
      timerRunning = false;  // Matikan timer setelah selesai
      timerFinished = true;  // Tandai bahwa timer sudah selesai
    }
  } else if (!timerFinished) {
    // Kondisi standby, saat timer belum berjalan atau setelah selesai
    Serial.println("Standby, tekan tombol untuk memulai timer.");
    delay(1000);  // Cek setiap detik
  }
}
