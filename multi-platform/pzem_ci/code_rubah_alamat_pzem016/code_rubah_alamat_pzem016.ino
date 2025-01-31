#include <ModbusMaster.h>
ModbusMaster node;
static uint8_t old_address = 1; //default address 0x01
static uint8_t new_address = 2;
 
#include <SoftwareSerial.h>
SoftwareSerial pzemSerial(23, 19);
 
void setup() {
  Serial.begin(115200);
  pzemSerial.begin(9600);
  Serial.println();
  delay(1000);
  for (int i = 0; i < 2; i++) {
    changeAddress(old_address, new_address);
    delay(5000);
  }
}
 
void loop() {
   
}
 
void changeAddress(uint8_t OldslaveAddr, uint8_t NewslaveAddr)
{
  static uint8_t SlaveParameter = 0x06;
  static uint16_t registerAddress = 0x0002; // Register address to be changed
  uint16_t u16CRC = 0xFFFF;
  u16CRC = crc16_update(u16CRC, OldslaveAddr);
  u16CRC = crc16_update(u16CRC, SlaveParameter);
  u16CRC = crc16_update(u16CRC, highByte(registerAddress));
  u16CRC = crc16_update(u16CRC, lowByte(registerAddress));
  u16CRC = crc16_update(u16CRC, highByte(NewslaveAddr));
  u16CRC = crc16_update(u16CRC, lowByte(NewslaveAddr));
 
  Serial.println("Changing Slave Address");
 
  pzemSerial.write(OldslaveAddr);
  pzemSerial.write(SlaveParameter);
  pzemSerial.write(highByte(registerAddress));
  pzemSerial.write(lowByte(registerAddress));
  pzemSerial.write(highByte(NewslaveAddr));
  pzemSerial.write(lowByte(NewslaveAddr));
  pzemSerial.write(lowByte(u16CRC));
  pzemSerial.write(highByte(u16CRC));
  
  Serial.print("respon: ");
  while (pzemSerial.available() > 0) {
    Serial.print(pzemSerial.read(), HEX);
    Serial.print(",");
  }
  Serial.println();
  delay(1000);
}
