#include <PZEM004Tv30.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define RXD2 16
#define TXD2 17

#if !defined(PZEM_SERIAL)
#define PZEM_SERIAL Serial2
#endif

#if defined(ESP32)
PZEM004Tv30 pzem_r(PZEM_SERIAL, RXD2, TXD2);
#elif defined(ESP8266)
#else
PZEM004Tv30 pzem_r(PZEM_SERIAL);
#endif

LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

float vr;
float ir;
float freq;
float pf_r;
float energy;
float power;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  lcd.init();  // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Hello, world!");
  lcd.setCursor(0, 1);
  lcd.print("Ywrobot Arduino!");
}

void loop() {

  lcd.clear();

  baca_pzem();
  Serial.print("Volt: ");
  Serial.print(vr, 2);
  Serial.print("V, ");
  Serial.print("curr: ");
  Serial.print(ir, 3);
  Serial.print("A, ");
  Serial.print("pf: ");
  Serial.print(pf_r);
  Serial.println("%, ");
  Serial.print("Power: ");
  Serial.print(power);
  Serial.print("W, ");
  Serial.print("Energy: ");
  Serial.print(energy, 3);
  Serial.print("kWh, ");
  Serial.print("freq: ");
  Serial.print(freq, 1);
  Serial.println("Hz, ");
  Serial.println();

  lcd.setCursor(0, 0);
  lcd.print("V= ");
  lcd.print(vr, 2);
  lcd.print(" Volt");
  lcd.setCursor(0, 1);
  lcd.print("I= ");
  lcd.print(ir, 2);
  lcd.print(" Amp");

  delay(2000);
}

void baca_pzem() {
  vr = pzem_r.voltage();
  if(isnan(vr)){
    vr = 0;
  }
  ir = pzem_r.current();
  freq = pzem_r.frequency();
  pf_r = pzem_r.pf();
  power = pzem_r.power();
  energy = pzem_r.energy();
}