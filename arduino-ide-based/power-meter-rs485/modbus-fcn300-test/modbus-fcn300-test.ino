#include <ModbusMaster.h>
#include <HardwareSerial.h>

// Konfigurasi Hardware Serial untuk RS485
HardwareSerial modbusSerial(2);  // Gunakan UART2 ESP32

ModbusMaster node;

// Alamat modbus device
static uint8_t modbusAddr = 1;

void setup() {
  modbusSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(115200);
  
  Serial.println("Fort FCN300 Raw Data Reader with Interpretation");
  Serial.println("==============================================");
  Serial.println("Membaca data mentah dan interpretasi");
  Serial.println();
}

void loop() {
  baca_data_raw("R", 0, 64);  // Baca 64 register mulai dari address 0
  delay(2000);
  Serial.println("==============================================");
}

void baca_data_raw(String fasa, uint16_t startAddr, uint16_t numRegisters) {
  Serial.print("Membaca Fasa ");
  Serial.print(fasa);
  Serial.print(" | Start: ");
  Serial.print(startAddr);
  Serial.print(" | Jumlah: ");
  Serial.print(numRegisters);
  Serial.println(" register");
  Serial.println("----------------------------------------------");

  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(startAddr, numRegisters);
  
  if (result == node.ku8MBSuccess) {
    // Tampilkan data HEX
    Serial.println("Data HEX:");
    for (int i = 0; i < numRegisters; i++) {
      uint16_t regValue = node.getResponseBuffer(i);
      Serial.print("0x");
      if (regValue < 0x1000) Serial.print("0");
      if (regValue < 0x0100) Serial.print("0");
      if (regValue < 0x0010) Serial.print("0");
      Serial.print(regValue, HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.println();
    
    // Tampilkan data per register
    Serial.println("Data per Register:");
    for (int i = 0; i < numRegisters; i++) {
      uint16_t registerValue = node.getResponseBuffer(i);
      Serial.print("Reg ");
      Serial.print(startAddr + i);
      Serial.print(": 0x");
      if (registerValue < 0x1000) Serial.print("0");
      if (registerValue < 0x0100) Serial.print("0");
      if (registerValue < 0x0010) Serial.print("0");
      Serial.print(registerValue, HEX);
      Serial.print(" | DEC: ");
      Serial.print(registerValue);
      Serial.println();
    }
    Serial.println();
    
    // Test interpretasi untuk setiap pasangan register
    Serial.println("INTERPRETASI DATA:");
    Serial.println("----------------------------------------------");
    
    for (int i = 0; i < numRegisters - 1; i += 2) {
      uint16_t highWord = node.getResponseBuffer(i);
      uint16_t lowWord = node.getResponseBuffer(i + 1);
      
      Serial.print(">>> Interpretasi Reg ");
      Serial.print(startAddr + i);
      Serial.print("-");
      Serial.print(startAddr + i + 1);
      Serial.println(" <<<");
      
      test_interpretations(highWord, lowWord);
      Serial.println("----------------------------------------------");
    }
    
  } else {
    Serial.print("Error membaca modbus: ");
    Serial.println(result);
    
    // Tampilkan error details
    switch (result) {
      case node.ku8MBIllegalFunction:
        Serial.println("Illegal Function");
        break;
      case node.ku8MBIllegalDataAddress:
        Serial.println("Illegal Data Address");
        break;
      case node.ku8MBIllegalDataValue:
        Serial.println("Illegal Data Value");
        break;
      case node.ku8MBSlaveDeviceFailure:
        Serial.println("Slave Device Failure");
        break;
      default:
        Serial.println("Unknown Error");
        break;
    }
  }
  
  Serial.println("----------------------------------------------");
  delay(1000);
}

void test_interpretations(uint16_t highWord, uint16_t lowWord) {
  // 1. Sebagai 32-bit integer (big-endian)
  int32_t asInt32BE = ((int32_t)highWord << 16) | lowWord;
  Serial.print("32-bit INT (BE): ");
  Serial.print(asInt32BE);
  Serial.print(" | HEX: 0x");
  Serial.println(((uint32_t)highWord << 16) | lowWord, HEX);
  
  // 2. Sebagai 32-bit integer (little-endian)
  int32_t asInt32LE = ((int32_t)lowWord << 16) | highWord;
  Serial.print("32-bit INT (LE): ");
  Serial.print(asInt32LE);
  Serial.print(" | HEX: 0x");
  Serial.println(((uint32_t)lowWord << 16) | highWord, HEX);
  
  // 3. Sebagai float (big-endian)
  float asFloatBE = convertToFloat(highWord, lowWord);
  Serial.print("Float (BE):      ");
  if (isnan(asFloatBE)) {
    Serial.print("NaN");
  } else {
    Serial.print(asFloatBE, 6);
  }
  Serial.println();
  
  // 4. Sebagai float (little-endian - word swap)
  float asFloatLE = convertToFloat(lowWord, highWord);
  Serial.print("Float (LE):      ");
  if (isnan(asFloatLE)) {
    Serial.print("NaN");
  } else {
    Serial.print(asFloatLE, 6);
  }
  Serial.println();
  
  // 5. Sebagai BCD encoded
  Serial.print("BCD High:        ");
  printBCD(highWord);
  Serial.print(" | BCD Low: ");
  printBCD(lowWord);
  Serial.println();
  
  // 6. Sebagai fixed point (skala 0.01)
  float asFixedPoint = asInt32BE * 0.01;
  Serial.print("Fixed Point 0.01: ");
  Serial.print(asFixedPoint, 2);
  Serial.println();
  
  // 7. Sebagai fixed point (skala 0.001)
  float asFixedPoint2 = asInt32BE * 0.001;
  Serial.print("Fixed Point 0.001: ");
  Serial.print(asFixedPoint2, 3);
  Serial.println();
  
  // 8. Sebagai two's complement
  Serial.print("Two's Complement: ");
  if (asInt32BE & 0x80000000) {
    Serial.print("-");
    Serial.print((~asInt32BE + 1) & 0x7FFFFFFF);
  } else {
    Serial.print(asInt32BE);
  }
  Serial.println();
}

float convertToFloat(uint16_t highWord, uint16_t lowWord) {
  union {
    uint32_t i;
    float f;
  } converter;
  
  converter.i = ((uint32_t)highWord << 16) | lowWord;
  return converter.f;
}

void printBCD(uint16_t value) {
  for (int i = 12; i >= 0; i -= 4) {
    uint8_t digit = (value >> i) & 0x0F;
    if (digit <= 9) {
      Serial.print(digit);
    } else {
      Serial.print("[");
      Serial.print(digit, HEX);
      Serial.print("]");
    }
  }
}

// Fungsi untuk test interpretasi khusus
void test_specific_interpretations() {
  // Test dengan data spesifik jika diperlukan
  Serial.println("=== TEST SPECIFIC INTERPRETATIONS ===");
  test_interpretations(0x22B8, 0x0000);
  Serial.println();
  test_interpretations(0x8701, 0x1388);
  Serial.println();
  test_interpretations(0x8201, 0x0BB8);
  Serial.println("=====================================");
}