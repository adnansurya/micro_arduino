#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>


//deklarasi variabel pin komponen
#define lembabPin A3
#define metal1Pin A0
#define metal2Pin A1
#define infra1Pin 6
#define infra2Pin 7
#define servo1Pin 9
#define servo2Pin 3


#define batasAdcLembab 100
#define batasAdcMetal 200

#define batasMetal 3
#define batasOrganik 4
#define batasNonOrganik 5


//deklarasi objek servo
Servo servo1;
Servo servo2;

LiquidCrystal_I2C lcd(0x27, 16, 2);  //deklarasi objek lcd


int sampahMetal = 0;
int sampahOrganik = 0;
int sampahNonOrganik = 0;


bool adaSampah = false;
bool nonMetal = false;

bool sampahPenuh = false;

int metal1Val;
int metal2Val;
int infra1Val;
int infra2Val;
int lembabVal;


void setup() {
  Serial.begin(115200);

  // lcd.init();
  lcd.begin();
  lcd.backlight();

  lcdPrint("Inisialisasi....", "", 500);

  pinMode(infra1Pin, INPUT);
  pinMode(infra2Pin, INPUT);

  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo1.write(90);
  servo2.write(90);

  lcdPrint("Sistem", "Standby", 1000);
}

void loop() {

  metal1Val = analogRead(metal1Pin);
  metal2Val = analogRead(metal2Pin);
  infra1Val = !digitalRead(infra1Pin);
  infra2Val = !digitalRead(infra2Pin);
  lembabVal = analogRead(lembabPin);


  debugSensor();

  adaSampah = infra1Val;
  if (adaSampah && sampahPenuh == false) {  //jika di infra1 ada objek
    lcdPrint("Sampah", "Terdeteksi!", 1000);
    debugSensor();
    delay(2000);
    if ((metal1Val > batasAdcMetal) && (metal2Val > batasAdcMetal)) {  //jika salah satu nilai adc berada di atas batas
      //objek adalah non metal
      lcdPrint("Kategori : ", "Sampah Non Metal", 2000);
      servo1.write(180);
      delay(2000);
      servo1.write(90);


    } else {
      //objek adalah metal
      lcdPrint("Kategori : ", "Sampah Metal", 2000);
      servo1.write(0);
      delay(2000);
      sampahMetal++;
      servo1.write(90);
      lcdStandby();
    }
  }

  nonMetal = infra2Val;
  if (nonMetal && sampahPenuh == false) {                      //jika di infra2 ada objek
    if (lembabVal > batasAdcLembab) {  //jika nilai persen lembab berada di atas batas
      //objek adalah organik
      lcdPrint("Kategori : ", "Sampah Organik", 2000);
      servo2.write(0);
      sampahOrganik++;

    } else {
      //objek adalah non-organik
      lcdPrint("Kategori : ", "Sampah Non-Organik", 2000);
      servo2.write(180);
      sampahNonOrganik++;
    }
    delay(2000);

    servo2.write(90);
    lcdStandby();
  }

  delay(1000);
}

void debugSensor() {
  Serial.print("metal1Val : ");
  Serial.print(metal1Val);
  Serial.print("\tmetal2Val : ");
  Serial.print(metal2Val);
  Serial.print("\tinfra1Val : ");
  Serial.print(infra1Val);
  Serial.print("\tinfra2Val : ");
  Serial.print(infra2Val);
  Serial.print("\tlembabVal : ");
  Serial.println(lembabVal);

  Serial.print("Metal : ");
  Serial.print(sampahMetal);
  Serial.print("\tOrganik : ");
  Serial.print(sampahOrganik);
  Serial.print("\tNon Organik : ");
  Serial.println(sampahNonOrganik);

  if (sampahPenuh == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("m1:");
    lcd.print(metal1Val);
    lcd.print(" m2:");
    lcd.print(metal2Val);

    lcd.setCursor(0, 1);
    lcd.print("i1:");
    lcd.print(infra1Val);
    lcd.print(" i2:");
    lcd.print(infra2Val);
    lcd.print(" L:");
    lcd.print(lembabVal);
  }
}


void lcdPrint(String baris1, String baris2, int jeda) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2);
  delay(jeda);
}


void lcdStandby() {

  if (sampahMetal >= batasMetal) {    
    Serial.println("SAMPAH METAL PENUH");
    sampahPenuh = true;

  } else if (sampahOrganik >= batasOrganik) {    
    Serial.println("SAMPAH ORGANIK PENUH");
    sampahPenuh = true;

  } else if (sampahNonOrganik >= batasNonOrganik) {    
    Serial.println("SAMPAH NON-ORGANIK PENUH");    
    sampahPenuh = true;
  } 
  
  if(sampahPenuh){
    lcdPrint(" Tempat Sampah", "     PENUH", 1000);
  }else {
    lcdPrint("Sistem", "Standby", 1000);
  }
}
