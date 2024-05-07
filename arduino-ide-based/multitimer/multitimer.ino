#include <LiquidCrystal_I2C.h>
#include "DHT.h"

#define I2C_ADDR    0x27
#define LCD_COLUMNS 16
#define LCD_LINES   2


#define DHTPIN 33 
#define DHTTYPE DHT22 

LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);
DHT dht(DHTPIN, DHTTYPE);

unsigned long lcdUpdateTime = 0;
unsigned long lcdUpdateInterval= 1000;

unsigned long phUpdateTime = 0;
unsigned long phUpdateInterval = 10000;

unsigned long dmsUpdateTime = 0;
unsigned long dmsUpdateInterval = 3000;

float f = 0.0;
float t = 0.0;
float h = 0.0;

bool dmsOn = true;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Hello, ESP32!");
   // Init
  lcd.init();
  lcd.backlight();
  
  dht.begin();

}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentTime = millis();
 


  // Check if any reads failed and exit early (to try again).
  
  if (currentTime - lcdUpdateTime >= lcdUpdateInterval) {
    h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }else{
      Serial.print("t: ");
      Serial.print(t);
      Serial.print("\th: ");
      Serial.println(h);
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("t:");
    lcd.print(t);
    lcd.setCursor(8, 0);
    lcd.print("h:");
    lcd.print(h);
    lcd.setCursor(0, 1);
    lcd.print("f:");
    lcd.print(f);
    lcdUpdateTime = currentTime;
  }

  if ((currentTime - phUpdateTime >= phUpdateInterval) && dmsOn == true) {
      // Read temperature as Fahrenheit (isFahrenheit = true)
    f = dht.readTemperature(true);
    if (isnan(f)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }else{
      Serial.print("f: ");
      Serial.println(f);
    }
    Serial.println("DMS Off");
    dmsOn = false;
    // phUpdateTime = currentTime;
    dmsUpdateTime = currentTime;
  }

  if ((currentTime - dmsUpdateTime >= dmsUpdateInterval) && dmsOn == false) {
      // Read temperature as Fahrenheit (isFahrenheit = true)
    Serial.println("DMS Standby");
    dmsOn = true;
    phUpdateTime = currentTime;
    // dmsUpdateTime = currentTime;
  }

  

  delay(10); // this speeds up the simulation
}
