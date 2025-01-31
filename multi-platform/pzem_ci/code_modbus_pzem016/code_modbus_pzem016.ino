#include <ModbusMaster.h>
#include <SoftwareSerial.h> 
#include <LiquidCrystal_I2C.h>
 
SoftwareSerial pzemSerial(23,19); //rx, tx
LiquidCrystal_I2C lcd(0x27, 16, 2);
ModbusMaster node;
static uint8_t pzemSlaveAddr = 0x03;
 
void setup() {
  pzemSerial.begin(9600); 
  Serial.begin(115200);
 // resetEnergy(pzemSlaveAddr);
  node.begin(pzemSlaveAddr, pzemSerial);
   lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Tes Pzem 016");
  delay(1000);
  lcd.clear();
}
 
/* 
RegAddr Description                 Resolution
0x0000  Voltage value               1LSB correspond to 0.1V       
0x0001  Current value low 16 bits   1LSB correspond to 0.001A
0x0002  Current value high 16 bits  
0x0003  Power value low 16 bits     1LSB correspond to 0.1W
0x0004  Power value high 16 bits  
0x0005  Energy value low 16 bits    1LSB correspond to 1Wh
0x0006  Energy value high 16 bits 
0x0007  Frequency value             1LSB correspond to 0.1Hz
0x0008  Power factor value          1LSB correspond to 0.01
0x0009  Alarm status  0xFFFF is alarm，0x0000is not alarm
*/
 
void loop() {
  uint8_t result;
  result = node.readInputRegisters(0x0, 9); //read the 9 registers of the PZEM-014 / 016
  lcd.clear();
  if (result ==node.ku8MBSuccess)
  {
    float voltage = node.getResponseBuffer(0x0000) / 10.0;
 
    uint32_t tempdouble = 0x00000000;
 
    float power;    
    tempdouble |= node.getResponseBuffer(0x0003);       //LowByte
    tempdouble |= node.getResponseBuffer(0x0004) << 8;  //highByte
    power = tempdouble / 10.0;
 
 
    float current;
    tempdouble = node.getResponseBuffer(0x0001);       //LowByte
    tempdouble |= node.getResponseBuffer(0x0002) << 8;  //highByte
    current = tempdouble / 1000.0;
    
    uint16_t energy;
    tempdouble = node.getResponseBuffer(0x0005);       //LowByte
    tempdouble |= node.getResponseBuffer(0x0006) << 8;  //highByte
    energy = tempdouble;
 
    Serial.print(voltage);
    Serial.print("V   ");
    lcd.setCursor(0, 0); lcd.print(String()+"V1:"+ voltage);
 
    Serial.print(current);
    Serial.print("A   ");
    lcd.setCursor(0, 1); lcd.print(String()+"A :"+ current );
   
    Serial.print(power);    
    Serial.print("W  ");
    lcd.setCursor(9, 0); lcd.print(String()+"P:"+ power );
    
    Serial.print(node.getResponseBuffer(0x0008));
    Serial.print("pf   ");
 
    Serial.print(energy);
    Serial.print("Wh  ");
    lcd.setCursor(9, 1); lcd.print(String()+"E:"+ energy );
    Serial.println();
 
  } else {
    Serial.println("Failed to read modbus");  
    lcd.setCursor(0, 0); lcd.print(String()+"Voltage Tidak ");
    lcd.setCursor(0, 1); lcd.print(String()+"Terhubung");
  }
  delay(2000);
}
 
void resetEnergy(uint8_t slaveAddr){
  //The command to reset the slave's energy is (total 4 bytes):
  //Slave address + 0x42 + CRC check high byte + CRC check low byte.
  uint16_t u16CRC = 0xFFFF;
  static uint8_t resetCommand = 0x42;
  u16CRC = crc16_update(u16CRC, slaveAddr);
  u16CRC = crc16_update(u16CRC, resetCommand);
  Serial.println("Resetting Energy");
  pzemSerial.write(slaveAddr);
  pzemSerial.write(resetCommand);
  pzemSerial.write(lowByte(u16CRC));
  pzemSerial.write(highByte(u16CRC));
  delay(1000);
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
  delay(1000);
}