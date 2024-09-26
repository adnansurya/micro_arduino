#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial14.h>
#include <fonts/SystemFont5x7.h>
#include <fonts/Arial_Black_16.h>
#include <fonts/Droid_Sans_16.h>


const int buttonPin = 2;              // Pin tombol
unsigned long startMillis;            // Variabel untuk menyimpan waktu mulai
unsigned long currentMillis;          // Variabel untuk menyimpan waktu saat ini
unsigned long previousMillis = 0; 
const unsigned long period = 600000;  // 10 menit dalam milidetik (600000 ms = 10 menit)
const unsigned long interval = 1000;  // 10 menit dalam milidetik (600000 ms = 10 menit)
bool timerRunning = false;            // Status apakah timer sedang berjalan
bool timerFinished = false;           // Status apakah timer sudah selesai

SoftDMD dmd(1, 1);  // DMD controls the entire display
// SPIDMD dmd(1,1);  // DMD controls the entire display
DMD_TextBox box(dmd, 0, 1);  // "box" provides a text box to automatically write to/scroll the display

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);  // Mengatur pin tombol dengan pull-up internal
  Serial.begin(9600);                // Memulai komunikasi serial
  Serial.println("Standby, tekan tombol untuk memulai timer.");
  dmd.setBrightness(255);
  dmd.selectFont(Droid_Sans_16);
  dmd.begin();
}

void loop() {

  currentMillis = millis();  // Dapatkan waktu saat ini

  // Membaca status tombol
  if (digitalRead(buttonPin) == LOW) {  // Tombol ditekan (LOW karena pull-up)
    if (!timerRunning) {
      startMillis = currentMillis;  // Simpan waktu mulai
      timerRunning = true;          // Mengaktifkan timer
      timerFinished = false;        // Reset status selesai
      Serial.println("Timer dimulai!");
      delay(200);  // Debouncing tombol
    }
  }

  if (currentMillis - previousMillis >= interval) {
     previousMillis = currentMillis;
    if (timerRunning) {


      if (currentMillis - startMillis < period) {
        box.clear();

        dmd.selectFont(Droid_Sans_16);
        unsigned long remainingTime = period - (currentMillis - startMillis);  // Waktu tersisa dalam milidetik
        unsigned int minutes = remainingTime / 60000;                          // Konversi ke menit
        unsigned int seconds = (remainingTime % 60000) / 1000;                 // Konversi ke detik

        // Tampilkan waktu dalam format mm:ss
        Serial.print("Sisa waktu: ");
        box.print(' ');
        // if (minutes < 10) {
        //   Serial.print('0');  // Tambahkan 0 jika menit kurang dari 10
        //   box.print('0');     // Tambahkan 0 jika menit kurang dari 10
        // }
        Serial.print(minutes);
        Serial.print(':');
        box.print(minutes);
        box.print(':');
        if (seconds < 10) {
          Serial.print('0');  // Tambahkan 0 jika detik kurang dari 10
          box.print('0');     // Tambahkan 0 jika detik kurang dari 10
        }
        Serial.println(seconds);
        box.print(seconds);



      } else {
        Serial.println("Timer selesai!");
        timerRunning = false;  // Matikan timer setelah selesai
        timerFinished = true;  // Tandai bahwa timer sudah selesai
      }
    }
  }

  delay(10);
}
