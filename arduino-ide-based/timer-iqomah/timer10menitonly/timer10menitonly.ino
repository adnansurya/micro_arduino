unsigned long startMillis;  // Variabel untuk menyimpan waktu mulai
unsigned long currentMillis; // Variabel untuk menyimpan waktu saat ini
const unsigned long period = 600000; // 10 menit dalam milidetik (600000 ms = 10 menit)

void setup() {
  Serial.begin(9600);  // Memulai komunikasi serial
  startMillis = millis();  // Simpan waktu saat mulai
}

void loop() {
  currentMillis = millis(); // Dapatkan waktu saat ini

  // Cek apakah waktu 10 menit sudah tercapai
  if (currentMillis - startMillis < period) {
    unsigned long remainingTime = period - (currentMillis - startMillis); // Waktu tersisa dalam milidetik
    unsigned int minutes = remainingTime / 60000; // Konversi ke menit
    unsigned int seconds = (remainingTime % 60000) / 1000; // Konversi ke detik

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
    while (true);  // Hentikan program setelah timer selesai
  }
}
