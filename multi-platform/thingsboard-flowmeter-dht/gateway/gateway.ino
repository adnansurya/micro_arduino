/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Modified from the examples of the Arduino LoRa library
  More resources: https://RandomNerdTutorials.com/esp32-lora-rfm95-transceiver-arduino-ide/
*********/

#include <SPI.h>
#include <LoRa.h>

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
  
  if (!LoRa.begin(915E6)) {
    Serial.println("LORA TIDAK JALAN");
    while(1);
  }
   // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  Serial.println("LoRa Initializing OK!");
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
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
  }
}
