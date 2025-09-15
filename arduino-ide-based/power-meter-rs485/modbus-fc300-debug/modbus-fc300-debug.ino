#include <ModbusMaster.h>
#include <HardwareSerial.h>

// Konfigurasi Hardware Serial untuk RS485
HardwareSerial modbusSerial(2);  // Gunakan UART2 ESP32

ModbusMaster node;

// Alamat modbus device
static uint8_t modbusAddr = 1;

// Variabel untuk start address yang dapat diubah
uint16_t startAddress = 0;
uint16_t numRegistersToRead = 64;
bool filterZero = false;
bool addressChanged = false;
bool numRegChanged = false;

void setup() {
  modbusSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(115200);
  
  Serial.println("Fort FCN300 Raw Data Reader with Interpretation");
  Serial.println("==============================================");
  Serial.println("Membaca data mentah dan interpretasi");
  Serial.println("Ketik 'A' + angka untuk mengubah start address (contoh: A100)");
  Serial.println("Ketik 'N' + angka untuk mengubah jumlah register (contoh: N32)");
  Serial.println("Ketik 'F' untuk toggle filter register 0/65535");
  Serial.println("Ketik 'D' untuk menampilkan setting saat ini");
  Serial.println("Default: Start=0, Jumlah=64, Filter=OFF");
  Serial.println();
}

void loop() {
  // Periksa input serial untuk mengubah setting
  checkSerialInput();
  
  // Jika address berubah, tampilkan informasi
  if (addressChanged) {
    Serial.print("Start address diubah menjadi: ");
    Serial.println(startAddress);
    addressChanged = false;
  }
  
  // Jika jumlah register berubah, tampilkan informasi
  if (numRegChanged) {
    Serial.print("Jumlah register diubah menjadi: ");
    Serial.println(numRegistersToRead);
    numRegChanged = false;
  }
  
  baca_data_raw("R", startAddress, numRegistersToRead);
  delay(2000);
  Serial.println("==============================================");
}

void checkSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim();
    
    if (input.length() > 0) {
      // Perintah untuk mengubah address
      if (input.charAt(0) == 'A' || input.charAt(0) == 'a') {
        if (input.length() > 1) {
          String addressStr = input.substring(1);
          addressStr.trim();
          
          // Konversi ke integer
          int newAddress = addressStr.toInt();
          
          if (newAddress >= 0 && newAddress <= 65535) {
            startAddress = (uint16_t)newAddress;
            addressChanged = true;
          } else {
            Serial.println("Error: Address harus antara 0-65535");
          }
        } else {
          Serial.println("Error: Format salah. Gunakan: A<angka> (contoh: A100)");
        }
      }
      // Perintah untuk mengubah jumlah register
      else if (input.charAt(0) == 'N' || input.charAt(0) == 'n') {
        if (input.length() > 1) {
          String numStr = input.substring(1);
          numStr.trim();
          
          // Konversi ke integer
          int newNum = numStr.toInt();
          
          if (newNum >= 1 && newNum <= 125) {
            numRegistersToRead = (uint16_t)newNum;
            numRegChanged = true;
          } else {
            Serial.println("Error: Jumlah register harus antara 1-125");
          }
        } else {
          Serial.println("Error: Format salah. Gunakan: N<angka> (contoh: N32)");
        }
      }
      // Perintah untuk toggle filter
      else if (input.charAt(0) == 'F' || input.charAt(0) == 'f') {
        filterZero = !filterZero;
        Serial.print("Filter register 0/65535: ");
        Serial.println(filterZero ? "ON" : "OFF");
      }
      // Perintah untuk menampilkan setting saat ini
      else if (input.charAt(0) == 'D' || input.charAt(0) == 'd') {
        displayCurrentSettings();
      }
      // Perintah bantuan
      else if (input.charAt(0) == 'H' || input.charAt(0) == 'h' || input == "?") {
        printHelp();
      }
      else {
        Serial.println("Perintah tidak dikenali. Ketik 'H' untuk bantuan.");
      }
    }
  }
}

void displayCurrentSettings() {
  Serial.println("=== SETTING SAAT INI ===");
  Serial.print("Start address: ");
  Serial.println(startAddress);
  Serial.print("Jumlah register: ");
  Serial.println(numRegistersToRead);
  Serial.print("Filter 0/65535: ");
  Serial.println(filterZero ? "ON" : "OFF");
  Serial.println("========================");
}

void printHelp() {
  Serial.println("=== PERINTAH YANG TERSEDIA ===");
  Serial.println("A<angka> - Ubah start address (contoh: A100)");
  Serial.println("N<angka> - Ubah jumlah register (contoh: N32)");
  Serial.println("F        - Toggle filter register 0/65535");
  Serial.println("D        - Tampilkan setting saat ini");
  Serial.println("H atau ? - Tampilkan bantuan ini");
  Serial.println("==============================");
}

void baca_data_raw(String fasa, uint16_t startAddr, uint16_t numRegisters) {
  Serial.print("Membaca Fasa ");
  Serial.print(fasa);
  Serial.print(" | Start: ");
  Serial.print(startAddr);
  Serial.print(" | Jumlah: ");
  Serial.print(numRegisters);
  Serial.print(" | Filter: ");
  Serial.println(filterZero ? "ON" : "OFF");
  Serial.println("----------------------------------------------");

  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(startAddr, numRegisters);
  
  if (result == node.ku8MBSuccess) {
    // Tampilkan data HEX (dengan filter jika aktif)
    Serial.println("Data HEX:");
    int hexCount = 0;
    for (int i = 0; i < numRegisters; i++) {
      uint16_t regValue = node.getResponseBuffer(i);
      
      // Filter jika aktif dan nilai adalah 0 atau 65535
      if (filterZero && (regValue == 0 || regValue == 65535)) {
        continue;
      }
      
      Serial.print("0x");
      if (regValue < 0x1000) Serial.print("0");
      if (regValue < 0x0100) Serial.print("0");
      if (regValue < 0x0010) Serial.print("0");
      Serial.print(regValue, HEX);
      Serial.print(" ");
      hexCount++;
      
      if (hexCount % 16 == 0) Serial.println();
    }
    if (hexCount == 0 && filterZero) {
      Serial.println("Semua register difilter (nilai 0 atau 65535)");
    }
    Serial.println();
    Serial.println();
    
    // Tampilkan data per register (dengan filter jika aktif)
    Serial.println("Data per Register:");
    int regCount = 0;
    for (int i = 0; i < numRegisters; i++) {
      uint16_t registerValue = node.getResponseBuffer(i);
      
      // Filter jika aktif dan nilai adalah 0 atau 65535
      if (filterZero && (registerValue == 0 || registerValue == 65535)) {
        continue;
      }
      
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
      regCount++;
    }
    
    if (regCount == 0 && filterZero) {
      Serial.println("Semua register difilter (nilai 0 atau 65535)");
    }
    Serial.println();
    
    // Test interpretasi untuk setiap pasangan register (dengan filter jika aktif)
    Serial.println("INTERPRETASI DATA:");
    Serial.println("----------------------------------------------");
    
    // Header tabel interpretasi
    Serial.println("Reg\t\t32-bit INT (BE)\t32-bit INT (LE)\tFloat (BE)\tFloat (LE)\tBCD High\tBCD Low\t\tFixed 0.01\tFixed 0.001\tTwo's Complement");
    Serial.println("-----------------------------------------------------------------------------------------------------------------------------------------------------------");
    
    int interpCount = 0;
    for (int i = 0; i < numRegisters - 1; i += 2) {
      uint16_t highWord = node.getResponseBuffer(i);
      uint16_t lowWord = node.getResponseBuffer(i + 1);
      
      // Filter jika aktif dan kedua nilai adalah 0 atau 65535
      if (filterZero && 
          ((highWord == 0 || highWord == 65535) && 
           (lowWord == 0 || lowWord == 65535))) {
        continue;
      }
      
      // Tampilkan interpretasi dalam format tabel
      print_interpretation_table(startAddr + i, highWord, lowWord);
      interpCount++;
    }
    
    if (interpCount == 0 && filterZero) {
      Serial.println("Semua pasangan register difilter (nilai 0 atau 65535)");
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

void print_interpretation_table(uint16_t regNum, uint16_t highWord, uint16_t lowWord) {
  // 1. Sebagai 32-bit integer (big-endian)
  int32_t asInt32BE = ((int32_t)highWord << 16) | lowWord;
  
  // 2. Sebagai 32-bit integer (little-endian)
  int32_t asInt32LE = ((int32_t)lowWord << 16) | highWord;
  
  // 3. Sebagai float (big-endian)
  float asFloatBE = convertToFloat(highWord, lowWord);
  
  // 4. Sebagai float (little-endian - word swap)
  float asFloatLE = convertToFloat(lowWord, highWord);
  
  // 5. Sebagai BCD encoded
  String bcdHigh = getBCDString(highWord);
  String bcdLow = getBCDString(lowWord);
  
  // 6. Sebagai fixed point (skala 0.01)
  float asFixedPoint01 = asInt32BE * 0.01;
  
  // 7. Sebagai fixed point (skala 0.001)
  float asFixedPoint001 = asInt32BE * 0.001;
  
  // 8. Sebagai two's complement
  String twosComplement;
  if (asInt32BE & 0x80000000) {
    twosComplement = "-" + String((~asInt32BE + 1) & 0x7FFFFFFF);
  } else {
    twosComplement = String(asInt32BE);
  }
  
  // Tampilkan dalam format tabel dengan tab
  Serial.print(regNum);
  Serial.print("-");
  Serial.print(regNum + 1);
  Serial.print("\t");
  
  Serial.print(asInt32BE);
  Serial.print("\t\t");
  
  Serial.print(asInt32LE);
  Serial.print("\t\t");
  
  if (isnan(asFloatBE)) {
    Serial.print("NaN");
  } else {
    Serial.print(asFloatBE, 6);
  }
  Serial.print("\t");
  
  if (isnan(asFloatLE)) {
    Serial.print("NaN");
  } else {
    Serial.print(asFloatLE, 6);
  }
  Serial.print("\t");
  
  Serial.print(bcdHigh);
  Serial.print("\t\t");
  
  Serial.print(bcdLow);
  Serial.print("\t\t");
  
  Serial.print(asFixedPoint01, 2);
  Serial.print("\t\t");
  
  Serial.print(asFixedPoint001, 3);
  Serial.print("\t\t");
  
  Serial.println(twosComplement);
}

String getBCDString(uint16_t value) {
  String result = "";
  for (int i = 12; i >= 0; i -= 4) {
    uint8_t digit = (value >> i) & 0x0F;
    if (digit <= 9) {
      result += String(digit);
    } else {
      result += "[";
      result += String(digit, HEX);
      result += "]";
    }
  }
  return result;
}

float convertToFloat(uint16_t highWord, uint16_t lowWord) {
  union {
    uint32_t i;
    float f;
  } converter;
  
  converter.i = ((uint32_t)highWord << 16) | lowWord;
  return converter.f;
}