#include <ModbusMaster.h>
#include <HardwareSerial.h>

// Konfigurasi Hardware Serial untuk RS485
HardwareSerial modbusSerial(2);  // Gunakan UART2 ESP32
ModbusMaster node;

// Alamat modbus device
static uint8_t modbusAddr = 1;

// Struktur untuk mendefinisikan register yang ingin dibaca
struct RegisterDefinition {
  uint16_t address;
  const char* name;
  const char* dataType;  // "U16", "S16", "U32", "S32", "FLOAT", "U32LE", "S32LE", "FLOATLE"
};

// Daftar register yang ingin dibaca (ubah sesuai kebutuhan)
// Gunakan format 0x untuk alamat hex
RegisterDefinition registersToRead[] = {
  { 0x0042, "A phase Voltage", "U32" },
  { 0x0044, "B phase Voltage", "U32" },
  { 0x0046, "C phase Voltage", "U32" },
  { 0x0058, "A phase Current", "U32" },
  { 0x005A, "B phase Current", "U32" },
  { 0x005C, "C phase Current", "U32" },
  { 0x004C, "AB line Voltage", "U32" },
  { 0x004E, "BC line Voltage", "U32" },
  { 0x0050, "CA line Voltage", "U32" },
  { 0x0080, "Frequency", "U16" },
  // Tambahkan register lain sesuai kebutuhan
};

const int numRegisters = sizeof(registersToRead) / sizeof(registersToRead[0]);

void setup() {
  modbusSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(115200);

  Serial.println("Fort FCN300 Static Register Reader");
  Serial.println("==================================");
  Serial.println("Membaca register yang telah ditentukan (HEX)");
  Serial.println();

  // Tampilkan daftar register yang akan dibaca
  Serial.println("Register yang akan dibaca:");
  for (int i = 0; i < numRegisters; i++) {
    Serial.print("Reg 0x");
    if (registersToRead[i].address < 0x1000) Serial.print("0");
    if (registersToRead[i].address < 0x0100) Serial.print("0");
    if (registersToRead[i].address < 0x0010) Serial.print("0");
    Serial.print(registersToRead[i].address, HEX);
    Serial.print(" (");
    Serial.print(registersToRead[i].name);
    Serial.print("): ");
    Serial.println(registersToRead[i].dataType);
  }
  Serial.println();
}

void loop() {
  baca_registers_statistik();
  delay(2000);
  Serial.println("==================================");
}

void baca_registers_statistik() {
  static uint32_t loopCount = 0;
  loopCount++;

  Serial.print("Pembacaan ke-");
  Serial.println(loopCount);
  Serial.println("----------------------------------");

  for (int i = 0; i < numRegisters; i++) {
    baca_single_register(registersToRead[i]);
  }
}

void baca_single_register(RegisterDefinition regDef) {
  // Tentukan jumlah register yang perlu dibaca berdasarkan tipe data
  int numRegsToRead = 1;
  if (strcmp(regDef.dataType, "U32") == 0 || strcmp(regDef.dataType, "S32") == 0 || strcmp(regDef.dataType, "FLOAT") == 0 || strcmp(regDef.dataType, "U32LE") == 0 || strcmp(regDef.dataType, "S32LE") == 0 || strcmp(regDef.dataType, "FLOATLE") == 0) {
    numRegsToRead = 2;
  }

  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(regDef.address, numRegsToRead);

  if (result == node.ku8MBSuccess) {
    Serial.print(regDef.name);
    Serial.print(" (Reg 0x");
    if (regDef.address < 0x1000) Serial.print("0");
    if (regDef.address < 0x0100) Serial.print("0");
    if (regDef.address < 0x0010) Serial.print("0");
    Serial.print(regDef.address, HEX);

    if (numRegsToRead > 1) {
      Serial.print("-0x");
      uint16_t endAddr = regDef.address + 1;
      if (endAddr < 0x1000) Serial.print("0");
      if (endAddr < 0x0100) Serial.print("0");
      if (endAddr < 0x0010) Serial.print("0");
      Serial.print(endAddr, HEX);
    }
    Serial.print("): ");

    // Interpretasi berdasarkan tipe data
    if (strcmp(regDef.dataType, "U16") == 0) {
      uint16_t value = node.getResponseBuffer(0);
      Serial.print(value);
      Serial.print(" (0x");
      if (value < 0x1000) Serial.print("0");
      if (value < 0x0100) Serial.print("0");
      if (value < 0x0010) Serial.print("0");
      Serial.print(value, HEX);
      Serial.println(")");
    } else if (strcmp(regDef.dataType, "S16") == 0) {
      int16_t value = (int16_t)node.getResponseBuffer(0);
      Serial.println(value);
    } else if (strcmp(regDef.dataType, "U32") == 0) {
      uint32_t value = ((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);
      Serial.println(value);
    } else if (strcmp(regDef.dataType, "S32") == 0) {
      int32_t value = ((int32_t)(int16_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);
      Serial.println(value);
    } else if (strcmp(regDef.dataType, "FLOAT") == 0) {
      float value = convertToFloat(node.getResponseBuffer(0), node.getResponseBuffer(1));
      Serial.println(value, 6);
    } else if (strcmp(regDef.dataType, "U32LE") == 0) {
      uint32_t value = ((uint32_t)node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
      Serial.println(value);
    } else if (strcmp(regDef.dataType, "S32LE") == 0) {
      int32_t value = ((int32_t)(int16_t)node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
      Serial.println(value);
    } else if (strcmp(regDef.dataType, "FLOATLE") == 0) {
      float value = convertToFloat(node.getResponseBuffer(1), node.getResponseBuffer(0));
      Serial.println(value, 6);
    }
  } else {
    Serial.print(regDef.name);
    Serial.print(": Error membaca - ");
    printModbusError(result);
  }
}

void printModbusError(uint8_t result) {
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
    case node.ku8MBInvalidSlaveID:
      Serial.println("Invalid Slave ID");
      break;
    case node.ku8MBInvalidFunction:
      Serial.println("Invalid Function");
      break;
    case node.ku8MBResponseTimedOut:
      Serial.println("Response Timeout");
      break;
    case node.ku8MBInvalidCRC:
      Serial.println("Invalid CRC");
      break;
    default:
      Serial.println("Unknown Error");
      break;
  }
}

float convertToFloat(uint16_t highWord, uint16_t lowWord) {
  union {
    uint32_t i;
    float f;
  } converter;

  converter.i = ((uint32_t)highWord << 16) | lowWord;
  return converter.f;
}

// Fungsi bantu untuk konversi string hex ke uint16_t
uint16_t hexStringToUint16(const char* hexString) {
  return (uint16_t)strtol(hexString, NULL, 16);
}