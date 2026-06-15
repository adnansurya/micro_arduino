#include <LiquidCrystal.h>
#include <Keypad.h>

// Konfigurasi Pin
const int soilPin = A0;
const int pumpPin = A2;

// Inisialisasi LCD (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);

// Konfigurasi Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {7, 6, 5, 4};
byte colPins[COLS] = {A4, A5, 2, 3}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Variabel Sistem
int threshold = 500; // Nilai ambang batas kelembaban (bisa diatur)
bool manualMode = false;

void setup() {
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, HIGH); // Asumsi relay aktif LOW
  lcd.begin(16, 2);
  lcd.print("Sistem Siap");
  delay(2000);
  lcd.clear();
}

void loop() {
  char key = keypad.getKey();
  int soilValue = analogRead(soilPin);

  // Perintah Manual dari Keypad
  if (key == 'A') { digitalWrite(pumpPin, LOW); manualMode = true; } // Pompa ON
  if (key == 'B') { digitalWrite(pumpPin, HIGH); manualMode = false; } // Pompa OFF
  
  // Masuk ke Menu Pengaturan
  if (key == 'D') {
    aturSensitivitas();
  }

  // Logika Otomatis (jika tidak dalam mode manual)
  if (!manualMode) {
    if (soilValue > threshold) {
      digitalWrite(pumpPin, LOW); // Pompa nyala
    } else {
      digitalWrite(pumpPin, HIGH); // Pompa mati
    }
  }

  // Tampilkan ke LCD
  lcd.setCursor(0, 0);
  lcd.print("Soil: "); lcd.print(soilValue);
  lcd.print("    ");
  lcd.setCursor(0, 1);
  lcd.print("Pmp:");
  lcd.print(digitalRead(pumpPin) == LOW ? " ON " : " OFF");
  lcd.print(" Set:"); lcd.print(threshold);
}

void aturSensitivitas() {
  lcd.clear();
  lcd.print("Set Threshold:");
  String input = "";
  
  while(true) {
    char key = keypad.getKey();
    if (key >= '0' && key <= '9') {
      input += key;
      lcd.setCursor(0, 1);
      lcd.print(input);
    }
    if (key == '#') { // Simpan
      if (input.length() > 0) threshold = input.toInt();
      break;
    }
  }
  lcd.clear();
}