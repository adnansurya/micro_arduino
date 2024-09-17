#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TCS3200.h> // Tambahkan library TCS3200

const int ph_Pin = A0;
const int oneWireBus = 2; // Pin untuk DS18B20
const int mq3_Pin = A1;   // Pin untuk MQ3

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
int nilai_analog_MQ3; // Variabel untuk nilai MQ3
unsigned int redFrequency, greenFrequency, blueFrequency; // Variabel untuk nilai warna

// Untuk kalibrasi
float PH4 = 1.526;
float PH7 = 3.199;

SoftwareSerial espSerial(10, 11); // RX, TX

void setup()
{
  pinMode(ph_Pin, INPUT);
  pinMode(mq3_Pin, INPUT); // Setel pin MQ3 sebagai input
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

  // Inisialisasi sensor warna
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  
  // Atur kecepatan frekuensi scaling ke 20%
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);
}

void loop()
{
  // Baca pH
  nilai_analog_PH = analogRead(ph_Pin);
  TeganganPh = 3.3 / 1024.0 * nilai_analog_PH;
  PH_step = (PH4 - PH7) / 3;
  Po = 7.00 + ((PH7 - TeganganPh) / PH_step);

  // Baca suhu
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);

  // Baca nilai MQ3
  nilai_analog_MQ3 = analogRead(mq3_Pin);

  // Baca nilai warna
  bacaWarna();

  // Tampilkan nilai sensor di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("pH: ");
  lcd.print(Po, 2);
  lcd.setCursor(9, 0);
  lcd.print("T: ");
  lcd.print(temperature, 1);
  lcd.setCursor(0, 1);
  lcd.print("MQ3: ");
  lcd.print(nilai_analog_MQ3);
  delay(1000);

  // Deteksi jenis cairan berdasarkan pH dan warna secara terpisah
  String jenisCairanPH = deteksiCairanPH(Po);
  String jenisCairanWarna = deteksiCairanWarna();

  // Tampilkan hasil deteksi pada LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(jenisCairanPH);
  lcd.setCursor(0, 1);
  lcd.print("W: ");
  lcd.print(jenisCairanWarna);
  delay(1000);

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

  delay(15000);
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

String deteksiCairanWarna() {
  // Baca nilai warna
  bacaWarna();

  if (redFrequency >= 14440 && redFrequency <= 14800 && 
      greenFrequency >= 10750 && greenFrequency <= 17000 && 
      blueFrequency >= 13873 && blueFrequency <= 23800) {
    return "Minyak Telon";
  } else if (redFrequency >= 10150 && redFrequency <= 16058 &&
             greenFrequency >= 11400 && greenFrequency <= 17345 &&
             blueFrequency >= 14653 && blueFrequency <= 23789) {
    return "Minyak Penyuling";
  } else {
    return "Tidak Dikenal";
  }
}

void bacaWarna() {
  // Baca nilai merah
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  redFrequency = pulseIn(sensorOut, LOW);

  // Baca nilai hijau
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  greenFrequency = pulseIn(sensorOut, LOW);

  // Baca nilai biru
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  blueFrequency = pulseIn(sensorOut, LOW);

  Serial.print("Merah: ");
  Serial.print(redFrequency);
  Serial.print(" Hijau: ");
  Serial.print(greenFrequency);
  Serial.print(" Biru: ");
  Serial.println(blueFrequency);
}
