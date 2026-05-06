#include <Arduino.h>


#define pinSoilMoist 35
#define pin_PH_tanah 34  // pin input untuk phADC
#define DMSpin 14        // pin output untuk DMS

float PH_tanah = 0;

float phHigh = 5.5;
int phADCHigh = 175;
float phLow = 4.0;
int phADCLow = 220;

float moist = 0.0;
int moistADCHigh = 3000;
int moistADCLow = 1000;

void ukurMoist() {
  int adcMoist = analogRead(pinSoilMoist);
  Serial.print("moistADC: ");
  Serial.println(adcMoist);
  moist = map(adcMoist, moistADCHigh, moistADCLow, 0, 10000) / 100.0;
  if (moist > 100.0) {
    moist = 100.0;
  }

  if (moist < 0.0) {
    moist = 0.0;
  }
  Serial.print("Moist: ");
  Serial.print(moist);
  Serial.println(" %");
}


void hitung_ph() {
  Serial.println();
  analogReadResolution(10);   // mengubah resolusi analog
  digitalWrite(DMSpin, LOW);  // aktifkan DMS

  delay(2 * 1000);  // wait DMS capture data
  int phADC = analogRead(pin_PH_tanah);
  if (phADC == 0 || phADC == 1023) {
    analogReadResolution(12);  // balik awal
    return;
  }

  Serial.print("phADC: ");
  Serial.println(phADC);

  float phA = phHigh * 100.0;
  float phB = phLow * 100.0;
  PH_tanah = map(phADC, phADCHigh, phADCLow, (int)phA, (int)phB) / 100.0;
  digitalWrite(DMSpin, HIGH);
  analogReadResolution(12);  // balik awal
  delay(3000);               // wait for DMS ready
}


void setup() {
  Serial.begin(9600);
  pinMode(DMSpin, OUTPUT);
  pinMode(pin_PH_tanah, INPUT);
}
void loop() {

  ukurMoist();

  hitung_ph();
  //saat gak stabil membuat nilai ph -1
  if (PH_tanah < 0) {
    PH_tanah = -1;
  }
  Serial.println("nilai PH_tanah : " + String(PH_tanah));
}