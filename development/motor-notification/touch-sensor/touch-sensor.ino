// --- KONFIGURASI PIN ---
#define TOUCH_PIN 4  

// Pin LED bawaan khusus Arduino Pro Micro (Active-LOW)
#define LED_RX 17
#define LED_TX 30

// --- VARIABEL TOUCH ADVANCED ---
unsigned long touchStartTime = 0;
unsigned long lastTouchReleaseTime = 0;
bool isTouching = false;
int touchCounter = 0;
const int longTouchDuration = 800;    
const int doubleTouchInterval = 300;  

void setup() {
  Serial.begin(115200);   
  Serial1.begin(115200);  
  
  pinMode(TOUCH_PIN, INPUT); 

  // Konfigurasi LED bawaan Pro Micro
  pinMode(LED_RX, OUTPUT);
  pinMode(LED_TX, OUTPUT);

  // Matikan kedua LED di awal (HIGH = MATI untuk Pro Micro)
  digitalWrite(LED_RX, HIGH);
  digitalWrite(LED_TX, HIGH);
  
  Serial1.println("Sistem Touch Pro Micro Siap dengan Indikator LED Terbalik...");
}

void loop() {
  // --- LOGIKA TOUCH SENSOR (SINGLE, DOUBLE, TRIPLE, LONG) ---
  bool currentTouch = digitalRead(TOUCH_PIN);

  if (currentTouch == HIGH && !isTouching) {
    isTouching = true;
    touchStartTime = millis();
  }

  if (currentTouch == LOW && isTouching) {
    isTouching = false;
    unsigned long duration = millis() - touchStartTime;

    if (duration >= longTouchDuration) {
      executeLongTouch();
      touchCounter = 0; 
    } else {
      touchCounter++;
      lastTouchReleaseTime = millis();
    }
  }

  if (touchCounter > 0 && (millis() - lastTouchReleaseTime > doubleTouchInterval)) {
    if (touchCounter == 1) {
      executeSingleTouch();
    } 
    else if (touchCounter == 2) {
      executeDoubleTouch();
    }
    else if (touchCounter >= 3) {
      executeTripleTouch();
    }
    
    touchCounter = 0; 
  }
}

// --- FUNGSI UNTUK BERKEDIP (NON-BLOCKING DELAY AGAR SMOOTH) ---
void blinkLED(int ledPin) {
  digitalWrite(ledPin, LOW);  // LOW = MENYALA pada Pro Micro
  delay(100);                 // Berkedip sejenak selama 100ms
  digitalWrite(ledPin, HIGH); // HIGH = MATI kembali
}

// --- FUNGSI AKSI (MENGIRIM TEKS & BERKEDIP) ---

void executeSingleTouch() {
  Serial1.println("SINGLE_TOUCH");
  Serial.println("Log USB: Terkirim -> SINGLE_TOUCH");
  blinkLED(LED_RX); // LED RX berkedip untuk Single Touch
}

void executeDoubleTouch() {
  Serial1.println("DOUBLE_TOUCH");
  Serial.println("Log USB: Terkirim -> DOUBLE_TOUCH");
  blinkLED(LED_TX); // LED TX berkedip untuk Double Touch
}

void executeTripleTouch() {
  Serial1.println("TRIPLE_TOUCH");
  Serial.println("Log USB: Terkirim -> TRIPLE_TOUCH");
  
  // Kedua LED berkedip bersamaan untuk Triple Touch
  digitalWrite(LED_RX, LOW);
  digitalWrite(LED_TX, LOW);
  delay(100);
  digitalWrite(LED_RX, HIGH);
  digitalWrite(LED_TX, HIGH);
}

void executeLongTouch() {
  Serial1.println("LONG_TOUCH");
  Serial.println("Log USB: Terkirim -> LONG_TOUCH");
  
  // Berkedip agak lama untuk menandakan Long Press
  digitalWrite(LED_RX, LOW);
  delay(300);
  digitalWrite(LED_RX, HIGH);
}