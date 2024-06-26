#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

#define suhuPin 13
#define relayPin 17

LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// Inisialisasi pin data (DQ)


OneWire oneWire(suhuPin);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin();
  lcd.init();  // initialize the lcd
  // Print a message to the LCD.
  lcd.backlight();

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
}

void loop() {
  sensors.requestTemperatures();                    // Minta pembacaan suhu
  float temperatureC = sensors.getTempCByIndex(0);  // Ambil suhu dalam derajat Celsius
  Serial.print("Suhu: ");
  Serial.print(temperatureC);
  Serial.println(" °C");
  lcdPrintAll("Suhu : ", String(temperatureC), 1000);
  // delay(1000);  // Tunggu 1 detik sebelum membaca lagi

  if(temperatureC < 32){
     digitalWrite(relayPin, HIGH);
  }else{
     digitalWrite(relayPin, LOW);
  }
}

void lcdPrintAll(String baris1, String baris2, int jeda) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2); 
  delay(jeda);
}
