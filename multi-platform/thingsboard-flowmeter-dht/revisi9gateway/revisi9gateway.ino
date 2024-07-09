#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// LoRa pins
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26

// LoRa frequency
#define BAND 915E6

// OLED pins
#define OLED_SDA 21
#define OLED_SCL 22 
#define OLED_RST 23
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Pin relay
const int relay1 = 15; // pin relay pertama
const int relay2 = 13; // pin relay kedua

// Waktu menyala relay1
const int relay1StartHour = 9;  // jam mulai menyala relay1
const int relay1StartMinute = 15;  // menit mulai menyala relay1
const int relay1EndHour = 9;  // jam berhenti menyala relay1
const int relay1EndMinute = 45;  // menit berhenti menyala relay1

// Waktu menyala relay2
const int relay2StartHour = 4;  // jam mulai menyala relay2
const int relay2StartMinute = 57;  // menit mulai menyala relay2
const int relay2EndHour = 4;  // jam berhenti menyala relay2
const int relay2EndMinute = 58;  // menit berhenti menyala relay2

//nilai analog kering dan basah
const int dryValue = 3000;        // Nilai analog saat tanah kering
const int wetValue = 1050;        // Nilai analog saat sensor terendam dalam air

// DHT22 pin
#define DHTPIN 25
#define DHTTYPE DHT22

// Soil Moisture Sensor pin
#define SOIL_MOISTURE_PIN 34

// Water Flow Sensor pins
#define FLOW_SENSOR1_PIN 2
#define FLOW_SENSOR2_PIN 4

// packet counter
int counter = 0;

// Flow pulse counters
volatile uint32_t flowPulseCount1 = 0;
volatile uint32_t flowPulseCount2 = 0;

// Calibration factor for water flow sensor (pulses per liter)
const float calibrationFactor = 7.5;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
RTC_DS3231 rtc;
DHT dht(DHTPIN, DHTTYPE);

int moisturePercentage;
int soilmoist;

// Array untuk menyimpan hari yang diizinkan untuk relay1 (0: Senin, 1: Selasa, dst.)
bool relay1AllowedDays[] = {false, false, false, false, false, false, false}; // Default: Semua hari dimatikan

// Interrupt service routines for flow sensors
void IRAM_ATTR flowSensor1ISR() {
  flowPulseCount1++;
}

void IRAM_ATTR flowSensor2ISR() {
  flowPulseCount2++;
}

void read_SoilMoist() {
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  int soilmoist = map(soilMoistureValue, dryValue, wetValue, 0, 100);
  if (soilmoist >= 100) {
    soilmoist = 100;
  } else if (soilmoist <= 0) {
    soilmoist = 0;
  }

  moisturePercentage = soilmoist;

  Serial.print("Nilai sensor kelembaban tanah: ");
  Serial.print(soilmoist);
  Serial.println("%");
}

void setup() {
  // initialize Serial Monitor
  Serial.begin(115200);
  
  // set relay pins as output
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);

  // Initialize allowed days for relay1
  relay1AllowedDays[0] = true; // Senin
  relay1AllowedDays[2] = true; // Rabu
  relay1AllowedDays[4] = true; // jumat
  
  // initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");
    while (1);
  }

  // Set RTC time to compile time if it lost power
  if (rtc.lostPower()) {
    Serial.println("RTC tidak diatur, mengatur waktu sekarang!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);

  // initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Initializing...");
  display.display();
  
  Serial.println("LoRa Sender Test");

  // SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);
  // setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initializing OK!");
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("LoRa Initializing OK!");
  display.display();
  delay(2000);

  // initialize DHT sensor
  dht.begin();

  // initialize flow sensors
  pinMode(FLOW_SENSOR1_PIN, INPUT_PULLUP);
  pinMode(FLOW_SENSOR2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR1_PIN), flowSensor1ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR2_PIN), flowSensor2ISR, RISING);
}

void loop() {
  // Ambil waktu sekarang dari RTC
  DateTime now = rtc.now();

  // Cek waktu untuk relay1
  if ((now.hour() > relay1StartHour || (now.hour() == relay1StartHour && now.minute() >= relay1StartMinute)) &&
      (now.hour() < relay1EndHour || (now.hour() == relay1EndHour && now.minute() < relay1EndMinute)) &&
      relay1AllowedDays[now.dayOfTheWeek() - 1]) { // Memeriksa apakah hari ini diizinkan untuk relay1
    digitalWrite(relay1, HIGH);  // Nyalakan relay1
  } else {
    digitalWrite(relay1, LOW);   // Matikan relay1
  }

  // Cek waktu untuk relay2
  if ((now.hour() > relay2StartHour || (now.hour() == relay2StartHour && now.minute() >= relay2StartMinute)) &&
      (now.hour() < relay2EndHour || (now.hour() == relay2EndHour && now.minute() < relay2EndMinute))) {
    digitalWrite(relay2, HIGH);  // Nyalakan relay2
  } else {
    digitalWrite(relay2, LOW);   // Matikan relay2
  }

  // Print waktu sekarang ke serial monitor
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  // Baca suhu dan kelembaban dari DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Cek apakah pembacaan berhasil
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Baca kelembaban tanah
  read_SoilMoist();

  // Hitung aliran air
  float flowRate1 = (flowPulseCount1 / calibrationFactor);
  float flowRate2 = (flowPulseCount2 / calibrationFactor);

  // Print suhu, kelembaban, kelembaban tanah, dan aliran air ke serial monitor
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C  Soil Moisture: "));
  Serial.print(moisturePercentage);
  Serial.print(F("%  Flow1: "));
  Serial.print(flowRate1);
  Serial.print(F(" L/min  Flow2: "));
  Serial.print(flowRate2);
  Serial.println(F(" L/min"));

  // Tampilkan waktu dan data sensor pada OLED
  display.clearDisplay();
  display.setCursor(0,0);
  display.print(now.year(), DEC);
  display.print('/');
  display.print(now.month(), DEC);
  display.print('/');
  display.print(now.day(), DEC);
  display.print(" ");
  display.print(now.hour(), DEC);
  display.print(':');
  display.print(now.minute(), DEC);
  display.print(':');
  display.println(now.second(), DEC);
  display.setCursor(0,10);
  display.print("Humid: ");
  display.print(h);
  display.println("%");
  display.setCursor(0,20);
  display.print("Temp: ");
  display.print(t);
  display.println("C");
  display.setCursor(0,30);
  display.print("Soil Moist: ");
  display.print(moisturePercentage);
  display.println("%");
  display.setCursor(0,40);
  display.print("Flow1: ");
  display.print(flowRate1);
  display.print(" L/min");
  display.setCursor(0,50);
  display.print("Flow2: ");
  display.print(flowRate2);
  display.print(" L/min");
  display.display();

  // LoRa communication
  Serial.print("Sending packet: ");
  Serial.println(counter);

  // Send LoRa packet to receiver
  LoRa.beginPacket();
  LoRa.print("rafa ");
  LoRa.print(counter);
  LoRa.print(" Temp: ");
  LoRa.print(t);
  LoRa.print("C Humidity: ");
  LoRa.print(h);
  LoRa.print("% Soil Moist: ");
  LoRa.print(soilmoist);
  LoRa.print("% Flow1: ");
  LoRa.print(flowRate1);
  LoRa.print(" L/min Flow2: ");
  LoRa.print(flowRate2);
  LoRa.print(" L/min");
  LoRa.endPacket();
  
  counter++;
  
  // Reset flow pulse counters
  flowPulseCount1 = 0;
  flowPulseCount2 = 0;
  
  delay(1000); // Delay 1 second before next loop
}
