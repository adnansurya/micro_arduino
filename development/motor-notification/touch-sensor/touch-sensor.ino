// --- KONFIGURASI PIN ---
#define TOUCH_PIN 4  // Menggunakan Pin 4 sesuai permintaan

// --- VARIABEL TOUCH ADVANCED ---
unsigned long touchStartTime = 0;
unsigned long lastTouchReleaseTime = 0;
bool isTouching = false;
int touchCounter = 0;
const int longTouchDuration = 800;    // Durasi untuk Long Press (800ms)
const int doubleTouchInterval = 300;  // Jeda maksimal antar ketukan (300ms)

void setup() {
  // Serial digunakan untuk debugging ke komputer via kabel USB (opsional)
  Serial.begin(115200); 
  
  // Serial1 digunakan untuk mengirim data via Pin TX/RX Pro Micro (Pin 1 & 0)
  Serial1.begin(115200); 
  
  // Set pin 4 sebagai input
  pinMode(TOUCH_PIN, INPUT); 
  
  // Kirim pesan siap ke Serial1
  Serial1.println("Sistem Touch Pro Micro Siap...");
}

void loop() {
  // --- LOGIKA TOUCH SENSOR (SINGLE, DOUBLE, LONG) ---
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
      touchCounter = 0; 
    } else {
      touchCounter++;
      lastTouchReleaseTime = millis();
    }
  }

  // 3. Evaluasi Ketukan setelah melewati jeda waktu (Timeout)
  if (touchCounter > 0 && (millis() - lastTouchReleaseTime > doubleTouchInterval)) {
    if (touchCounter == 1) {
      // === ACTION: SINGLE TOUCH ===
      executeSingleTouch();
    } 
    else if (touchCounter >= 2) {
      // === ACTION: DOUBLE TOUCH ===
      executeDoubleTouch();
    }
    
    touchCounter = 0; 
  }
}

// --- FUNGSI AKSI (MENGIRIM TEKS VIA SERIAL1) ---

void executeSingleTouch() {
  // Mengirim teks aksi melalui Serial1 (Pin TX)
  Serial1.println("SINGLE_TOUCH");
  
  // Tetap dimonitor di komputer via USB (bisa dihapus jika tidak butuh)
  Serial.println("Log USB: Terkirim via Serial1 -> SINGLE_TOUCH");
}

void executeDoubleTouch() {
  Serial1.println("DOUBLE_TOUCH");
  Serial.println("Log USB: Terkirim via Serial1 -> DOUBLE_TOUCH");
}

void executeLongTouch() {
  Serial1.println("LONG_TOUCH");
  Serial.println("Log USB: Terkirim via Serial1 -> LONG_TOUCH");
}