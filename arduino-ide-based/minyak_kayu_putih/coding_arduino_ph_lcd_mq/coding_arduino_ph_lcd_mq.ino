#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int ph_Pin = A0;
const int oneWireBus = 2;  // Pin untuk DS18B20
const int mq3_Pin = A1;    // Pin untuk MQ3

// Inisialisasi sensor
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Pin TCS3200
const int S0 = 4;
const int S1 = 5;
const int S2 = 6;
const int S3 = 7;
const int sensorOut = 8;

float Po = 0;
float PH_step;
int nilai_analog_PH;
double TeganganPh;
int nilai_analog_MQ3;                                      // Variabel untuk nilai MQ3
unsigned int redFrequency, greenFrequency, blueFrequency;  // Variabel untuk nilai warna



SoftwareSerial espSerial(10, 11);  // RX, TX

void setup() {
  pinMode(ph_Pin, INPUT);
  pinMode(mq3_Pin, INPUT);  // Setel pin MQ3 sebagai input
  Serial.begin(9600);
  espSerial.begin(9600);

  // Inisialisasi sensor suhu
  sensors.begin();

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("PH Meter");
  delay(2000);
  lcd.clear();

  // Atur kecepatan frekuensi scaling ke 20%
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);
}

void loop() {
  // Baca pH
  nilai_analog_PH = analogRead(ph_Pin);
  Po = 26.0708 - ((0.0323125) * (float)nilai_analog_PH);

  // Baca suhu
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);

  // Baca nilai MQ3
  nilai_analog_MQ3 = analogRead(mq3_Pin);

  // Tampilkan nilai sensor di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MQ 3 : ");
  lcd.print(nilai_analog_MQ3);
  lcd.setCursor(0, 1);
  lcd.print("Suhu : ");
  lcd.print(temperature, 1);

  delay(1000);

  // Deteksi jenis cairan berdasarkan pH dan warna secara terpisah
  String jenisCairanPH = deteksiCairanPH(Po);
  String jenisCairanWarna = "";

  // Tampilkan hasil deteksi pada LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("pH : ");
  lcd.print(Po, 2);
  lcd.setCursor(0, 1);  
  lcd.print(jenisCairanPH);


  Serial.print("PH:");
  Serial.print(Po, 2);
  Serial.print("\n");
  Serial.print("Temp:");
  Serial.print(temperature);
  Serial.print("\n");
  Serial.print("MQ3:");
  Serial.print(nilai_analog_MQ3);
  Serial.print("\n");
  Serial.print("Jenis:");
  Serial.print(jenisCairanPH);
  Serial.print("\n");

    // Kirim data ke ESP8266
    espSerial.print("PH:");
    espSerial.print(Po, 2);
    espSerial.print("\n");
    espSerial.print("Temp:");
    espSerial.print(temperature);
    espSerial.print("\n");
    espSerial.print("MQ3:");
    espSerial.print(nilai_analog_MQ3);
    espSerial.print("\n");
    espSerial.print("Merah:");
    espSerial.print(redFrequency);
    espSerial.print("\n");
    espSerial.print("Hijau:");
    espSerial.print(greenFrequency);
    espSerial.print("\n");
    espSerial.print("Biru:");
    espSerial.print(blueFrequency);
    espSerial.print("\n");
  

  delay(1000);
}

String deteksiCairanPH(float Po) { 
    if (Po > 6.50) {
      return "Minyak Telon";
    } else if (Po >= 5.00 && Po <= 6.50) {
      return "Minyak Penyuling";
    } else {
      return "Tidak Dikenal";
    }
  
}
