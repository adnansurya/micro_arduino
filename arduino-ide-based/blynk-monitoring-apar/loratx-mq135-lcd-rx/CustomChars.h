#ifndef CUSTOM_CHARS_H
#define CUSTOM_CHARS_H

#include <LiquidCrystal_I2C.h>

// Definisi karakter kustom
byte arrowUp[8] = {
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b00100,
  0b00100,
  0b00000,
  0b00000
};

byte arrowDown[8] = {
  0b00000,
  0b00100,
  0b00100,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
  0b00000
};

byte arrowLeft[8] = {
  0b00100,
  0b01000,
  0b11111,
  0b01000,
  0b00100,
  0b00000,
  0b00000,
  0b00000
};

byte arrowRight[8] = {
  0b00100,
  0b00010,
  0b11111,
  0b00010,
  0b00100,
  0b00000,
  0b00000,
  0b00000
};

// Fungsi untuk menginisialisasi karakter kustom
void initializeCustomChars(LiquidCrystal_I2C &lcd) {
  lcd.createChar(0, arrowUp);
  lcd.createChar(1, arrowDown);
  lcd.createChar(2, arrowLeft);
  lcd.createChar(3, arrowRight);
}

#endif
