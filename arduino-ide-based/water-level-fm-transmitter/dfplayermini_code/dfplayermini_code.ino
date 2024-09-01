//  ESP32dfmini01
//
#include "HardwareSerial.h"
#include "DFRobotDFPlayerMini.h"

const byte RXD2 = 16; // Connects to module's RX 
const byte TXD2 = 17; // Connects to module's TX 

HardwareSerial dfSD(1); // Use UART channel 1
DFRobotDFPlayerMini player;
   
void setup() 
{
  Serial.begin(115200);
  dfSD.begin(9600, SERIAL_8N1, RXD2, TXD2);  // 16,17
  delay(5000);

  if (player.begin(dfSD)) 
  {
    Serial.println("OK");

    // Set volume to maximum (0 to 30).
    player.volume(17); //30 is very loud
  } 
  else 
  {
    Serial.println("Connecting to DFPlayer Mini failed!");
  }
}

void loop() 
{
  Serial.println("Playing #1");
  player.play(1);
  Serial.println("play start");
  delay(5000);
  Serial.println("played");
  delay(1000);

  Serial.println("Playing #2");
  player.play(2);
  Serial.println("play start");
  delay(10000);
  Serial.println("played");
  delay(1000);

  Serial.println("Playing #3");
  player.play(3);
  Serial.println("play start");
  delay(10000);
  Serial.println("played");
  delay(1000);

  Serial.println("Playing #4");
  player.play(4);
  Serial.println("play start");
  delay(10000);
  Serial.println("played\r\n\r\n");
  delay(1000);    
}