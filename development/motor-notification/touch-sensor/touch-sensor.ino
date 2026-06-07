// --- KONFIGURASI PIN ---
#define TOUCH_PIN 4  

// --- VARIABEL TOUCH ADVANCED ---
unsigned long touchStartTime = 0;
unsigned long lastTouchReleaseTime = 0;
bool isTouching = false;
int touchCounter = 0;
const int longTouchDuration = 800;    // Durasi untuk Long Press (800ms)
const int doubleTouchInterval = 300;  // Jeda maksimal antar ketukan (300ms)

void setup() {
  Serial.begin(115200);   // Untuk debugging via USB komputer
  Serial1.begin(115200);  // Untuk kirim data via PIN TX fisik (Pin 1)
  
  pinMode(TOUCH_PIN, INPUT); 
  Serial1.println("Sistem Touch Pro Micro (Plus Triple Touch) Siap...");
}

void loop() {
  // --- LOGIKA TOUCH SENSOR (SINGLE, DOUBLE, TRIPLE, LONG) ---
  bool currentTouch = digitalRead(TOUCH_PIN);

  // 1. Deteksi saat sensor MULAI DITENTUH
  if (currentTouch == HIGH && !isTouching) {
    isTouching = true;
    touchStartTime = millis();
  }

  // 2. Deteksi saat sentuhan DILEPAS
  if (currentTouch == LOW && isTouching) {
    isTouching = false;
    unsigned long duration = millis() - touchStartTime;

    if (duration >= longTouchDuration) {
      // === ACTION: LONG TOUCH ===
      executeLongTouch();
      touchCounter = 0; // Reset counter agar tidak mengganggu hitungan ketukan pendek
    } else {
      // Jika ketukan pendek, tambahkan hitungan
      touchCounter++;
      lastTouchReleaseTime = millis();
    }
  }

  // 3. Evaluasi Ketukan setelah melewati jeda waktu toleransi (Timeout)
  if (touchCounter > 0 && (millis() - lastTouchReleaseTime > doubleTouchInterval)) {
    if (touchCounter == 1) {
      // === ACTION: SINGLE TOUCH ===
      executeSingleTouch();
    } 
    else if (touchCounter == 2) {
      // === ACTION: DOUBLE TOUCH ===
      executeDoubleTouch();
    }
    else if (touchCounter >= 3) {
      // === ACTION: TRIPLE TOUCH ===
      executeTripleTouch();
    }
    
    touchCounter = 0; // Reset counter setelah aksi berhasil dieksekusi
  }
}

// --- FUNGSI AKSI (MENGIRIM TEKS VIA SERIAL1) ---

void executeSingleTouch() {
  Serial1.println("SINGLE_TOUCH");
  Serial.println("Log USB: Terkirim -> SINGLE_TOUCH");
}

void executeDoubleTouch() {
  Serial1.println("DOUBLE_TOUCH");
  Serial.println("Log USB: Terkirim -> DOUBLE_TOUCH");
}

void executeTripleTouch() {
  Serial1.println("TRIPLE_TOUCH");
  Serial.println("Log USB: Terkirim -> TRIPLE_TOUCH");
}

void executeLongTouch() {
  Serial1.println("LONG_TOUCH");
  Serial.println("Log USB: Terkirim -> LONG_TOUCH");
}