#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// --- KONFIGURASI KEYPAD 4x4 ---
const byte ROWS = 4; // empat baris
const byte COLS = 4; // empat kolom

// Tambahkan variabel untuk mengontrol kapan update display dilakukan
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 100; // Update setiap 100 ms

// Karakter Keypad
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// PIN Keypad (sesuaikan dengan koneksi Anda ke Arduino)
// Baris (Row Pins) terhubung ke pin digital Arduino
byte rowPins[ROWS] = {7, 6, 5, 4}; 
// Kolom (Column Pins) terhubung ke pin digital Arduino
byte colPins[COLS] = {3, 2, 11, 10}; 

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// --- KONFIGURASI LCD 16x2 I2C ---
// Ganti 0x27 jika alamat I2C LCD Anda berbeda (bisa 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- VARIABEL GAME ---
enum GameState {
  START_SCREEN,
  PLAYING,
  GAME_OVER
};
GameState gameState = START_SCREEN;

int score = 0;
char targetKey;
unsigned long reactionTimeLimit = 3000; // Durasi awal: 3000 ms (3 detik)
unsigned long startTime;
const unsigned long reductionAmount = 200; // Pengurangan waktu reaksi per level: 200 ms
const unsigned long minTimeLimit = 800; // Batas minimal waktu reaksi: 800 ms (0.8 detik)


// --- FUNGSI UTAMA ---

void setup() {
  Serial.begin(9600); // Untuk debugging
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  
  showStartScreen();
}

void loop() {
  char key = customKeypad.getKey();

  switch (gameState) {
    case START_SCREEN:
      if (key == '#') {
        initGame();
      }
      break;

    case PLAYING:
      if (key != NO_KEY) {
        checkReaction(key);
      }

      // Panggil fungsi untuk memperbarui tampilan waktu secara berkala
      if (millis() - lastDisplayUpdate >= displayUpdateInterval) {
          updateDisplay();
          lastDisplayUpdate = millis();
      }
      
      // Cek Batas Waktu
      if (millis() - startTime >= reactionTimeLimit) {
        endGame();
      }
      break;
      
    case GAME_OVER:
      // Kembali ke Start Screen setelah 10 detik (10000 ms)
      if (millis() - startTime >= 10000) {
        showStartScreen();
      }
      break;
  }
}

// --- FUNGSI GAME ---

/**
 * Menampilkan layar pembuka dan mereset status game.
 */
void showStartScreen() {
  gameState = START_SCREEN;
  score = 0;
  reactionTimeLimit = 3000;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("REACTION GAME!");
  lcd.setCursor(0, 1);
  lcd.print("Tekan # utk Mulai");
}

/**
 * Menginisialisasi variabel dan memulai permainan.
 */
void initGame() {
  gameState = PLAYING;
  score = 0;
  reactionTimeLimit = 3000;
  
  generateNewTarget();
}

/**
 * Mengacak karakter baru dan memulai hitungan waktu.
 */
void generateNewTarget() {
  // Hanya ambil karakter 0-9 dan A-D (Total 14 karakter)
  int randIndex = random(0, 14); 
  // Gunakan karakter dari array hexaKeys
  if (randIndex < 10) {
    targetKey = hexaKeys[randIndex / 4][randIndex % 4];
  } else {
    // A, B, C, D berada di kolom 3 (indeks 3)
    targetKey = hexaKeys[randIndex - 10][3]; 
  }
  
  // Clear dan Tampilkan Target (Hanya Teks Tetap) di LCD
  lcd.clear();  
  lcd.setCursor(0, 0);
  lcd.print("Tekan: "); // Baris 0: TARGET
  lcd.print(targetKey);
 
  
  // Mulai hitungan waktu reaksi
  startTime = millis();

  // Perbarui tampilan segera setelah target baru dihasilkan
  updateDisplay();
}

/**
 * Memperbarui tampilan skor dan waktu tersisa.
 */
void updateDisplay() {
  if (gameState != PLAYING) return;
  
  // Hitung waktu yang tersisa (dalam milidetik)
  unsigned long elapsedTime = millis() - startTime;
  long remainingTime = reactionTimeLimit - elapsedTime;
  
  // Pastikan waktu tidak negatif
  if (remainingTime < 0) remainingTime = 0;
  
  // Konversi waktu tersisa ke detik dengan dua desimal
  float remainingSeconds = (float)remainingTime / 1000.0;
  
  // Tampilkan Skor (Diupdate setiap kali updateDisplay dipanggil)
  lcd.setCursor(8, 1);
  lcd.print("SKOR:");
  lcd.print(score);
  // Bersihkan sisa karakter lama jika skor berubah dari dua digit ke satu digit, dst.
  lcd.print(" "); 

  // Tampilkan Waktu Tersisa
  lcd.setCursor(0, 1);
  
  // Gunakan 4 karakter (misal: 3.00, 0.80) untuk waktu
  // Menampilkan waktu dalam format x.xx (2 desimal)  
  lcd.print(remainingSeconds, 2); 
  lcd.print("s"); // Satuan waktu
  lcd.print(" "); // Bersihkan sisa karakter (penting untuk waktu yang mengecil)
}

/**
 * Memeriksa tombol yang ditekan oleh pemain.
 */
void checkReaction(char pressedKey) {
  if (pressedKey == targetKey) {
    // 1. Benar, Tambah Skor
    score++;
    
    // 2. Kurangi Batas Waktu (membuat game lebih cepat)
    if (reactionTimeLimit > minTimeLimit) {
      reactionTimeLimit -= reductionAmount;
    }

    // 3. Lanjutkan ke Target Baru
    generateNewTarget();

  } else if (pressedKey != NO_KEY && pressedKey != targetKey) {
    // Salah menekan tombol (Game Over)
    endGame();
  }
}

/**
 * Mengakhiri permainan dan menampilkan skor.
 */
void endGame() {
  gameState = GAME_OVER;
  
  // Tampilkan Skor Akhir
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("GAME OVER!");
  lcd.setCursor(0, 1);
  lcd.print("SKOR AKHIR: ");
  lcd.print(score);
  
  // Gunakan startTime untuk hitungan mundur 10 detik
  startTime = millis();
}