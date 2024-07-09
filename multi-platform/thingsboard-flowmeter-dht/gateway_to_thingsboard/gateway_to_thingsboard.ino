/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Modified from the examples of the Arduino LoRa library
  More resources: https://RandomNerdTutorials.com/esp32-lora-rfm95-transceiver-arduino-ide/
*********/

#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED pins
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST 23
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels


//define the pins used by the transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26

//id lora yang diizinkan
const char* lora_rafa = "rafa";
const char* lora_raply = "raply";

//variabel untuk menampung data yang dikirim dari lora
float hum = 0.0;
float temp = 0.0;
float moist = 0.0;
float flowrate1 = 0.0;
float flowrate2 = 0.0;
long rssiVal;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

void setup() {
  //initialize Serial Monitor
  Serial.begin(115200);

  //setting pin
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);

  //replace the LoRa.begin(---E-) argument with your location's frequency
  //433E6 for Asia
  //868E6 for Europe
  //915E6 for North America

  // initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) {  // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Initializing...");
  display.display();

  if (!LoRa.begin(915E6)) {
    Serial.println("LORA TIDAK JALAN");
    while (1)
      ;
  }
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  Serial.println("LoRa Initializing OK!");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("LoRa Initializing OK!");
  display.display();
  delay(2000);
}

void loop() {
  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Menerima Paket:   ");

    // read packet
    while (LoRa.available()) {
      String sensorData = LoRa.readString();
      if (sensorData.startsWith(lora_rafa)) {
        Serial.println("Diterima dari " + String(lora_rafa) + ": " + sensorData);
      }
      if (sensorData.startsWith(lora_raply)) {
        Serial.println("Diterima dari " + String(lora_raply) + ": " + sensorData);
      }
      hum = extractFromRawString(sensorData, "Humidity:", "% Soil");
      temp = extractFromRawString(sensorData, "Temp:", "C");
      moist = extractFromRawString(sensorData, "Soil Moist:", "% Flow");
      flowrate1 = extractFromRawString(sensorData, "Flow1:", "L/min");
      flowrate2 = extractFromRawString(sensorData, "Flow2:", "L/min");
      rssiVal = LoRa.packetRssi();
      refreshDisplay();
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(rssiVal);
  }
}

float extractFromRawString(String sourceStr, String startWith, String endsWith) {
  String extractedStr = "";

  int firstIndex = sourceStr.indexOf(startWith) + startWith.length();
  int lastIndex = sourceStr.indexOf(endsWith);

  extractedStr = sourceStr.substring(firstIndex, lastIndex);  
  extractedStr.trim();
  return extractedStr.toFloat();
}


void refreshDisplay() {
  // Tampilkan waktu dan data sensor pada OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Humid: ");
  display.print(hum);
  display.println("%");
  display.setCursor(0, 10);
  display.print("Temp: ");
  display.print(temp);
  display.println("C");
  display.setCursor(0, 20);
  display.print("Soil Moist: ");
  display.print(moist);
  display.println("%");
  display.setCursor(0, 30);
  display.print("Flow1: ");
  display.print(flowrate1);
  display.print(" L/min");
  display.setCursor(0, 40);
  display.print("Flow2: ");
  display.print(flowrate2);
  display.print(" L/min");
  display.setCursor(0, 50);
  display.print("RSSI: ");
  display.print(rssiVal);
  display.print(" dBm");
  display.display();
}
