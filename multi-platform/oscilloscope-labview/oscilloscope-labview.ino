#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Encoder.h>
#include <LiquidCrystal_I2C.h>

// Inisialisasi Perangkat
Adafruit_MCP4725 dac;
Encoder myEnc(2, 3);
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Alamat I2C umum: 0x27 atau 0x3F

// Variabel Kontrol
long oldPosition = -999;
int waveMode = 0;
int counter = 0;
unsigned long lastLCDUpdate = 0;

// Nama-nama Gelombang
const char* waveNames[] = { "SINUS", "SEGITIGA", "GERGAJI", "KOTAK" };

float ampScale;
float freqDisplay;

void setup() {
  dac.begin(0x60);
  Wire.setClock(400000);  // Mode cepat

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Signal Generator");
  delay(1000);
  lcd.clear();
}

void loop() {
  // 1. Baca Rotary Encoder (Mode Gelombang)


  // 2. Baca Potensio
  int potFreq = analogRead(A0);
  int potAmp = analogRead(A1);

  ampScale = potAmp / 1023.0;
  int freqDelay = map(potFreq, 0, 1023, 500, 5000);
  int freqBuff = map(potFreq, 0, 1023, 2500, 440);
  freqDisplay = freqBuff / 100.0;

  // 3. Update LCD (Setiap 300ms agar tidak lag)
  if (millis() - lastLCDUpdate > 300) {
    long newPosition = myEnc.read() / 4;
    if (newPosition != oldPosition) {
      oldPosition = newPosition;
      waveMode = abs(newPosition % 4);  // Sekarang ada 4 mode (0, 1, 2, 3)
    }
    updateDisplay(waveNames[waveMode], map(potAmp, 0, 1023, 0, 100), freqDelay);
    lastLCDUpdate = millis();
  }

  // 4. Kalkulasi & Kirim Sinyal
  uint32_t outputVal = 0;
  if (waveMode == 0) {  // SINUS
    outputVal = (sin(counter * 0.15) + 1) * 2047.5;
  } else if (waveMode == 1) {  // SEGITIGA
    outputVal = (counter % 50 < 25) ? (counter % 25) * 163 : (25 - (counter % 25)) * 163;
  } else if (waveMode == 2) {
    outputVal = (counter % 50) * 81;
  } else if (waveMode == 3) {  // GELOMBANG PERSEGI
    // Jika counter di bawah 25 (setengah siklus), set ke MAX, jika lebih set ke 0
    outputVal = (counter % 50 < 25) ? 4095 : 0;
  }

  dac.setVoltage((uint16_t)(outputVal * ampScale), false);

  delayMicroseconds(freqDelay);
  counter++;
  if (counter > 1000) counter = 0;
}

void updateDisplay(const char* mode, int amp, int freq) {
  lcd.setCursor(0, 0);
  lcd.print("Mode: ");
  lcd.print(mode);
  lcd.print("    ");  // Clear sisa karakter

  lcd.setCursor(0, 1);
  // lcd.print("A:");
  lcd.print(ampScale * 5.0);
  lcd.print("V  ");
  lcd.print(freqDisplay);
  lcd.print("Hz  ");
}