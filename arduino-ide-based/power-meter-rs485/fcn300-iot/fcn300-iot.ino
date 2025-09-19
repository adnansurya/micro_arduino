#include <ModbusMaster.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// Konfigurasi Hardware Serial untuk RS485
HardwareSerial modbusSerial(2);  // Gunakan UART2 ESP32
ModbusMaster node;

// Konfigurasi WiFi
const char* ssid = "DAMAI KEMAKMURAN";
const char* password = "damai321";

// URL Web App
const String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbw5HivqIgz4phUOFdWfPoQDfc_3VPm3T9pqjfDVJxdEw1ODb4Po_AO-k49P9BrhJah4nA/exec";

// Alamat modbus device
static uint8_t modbusAddr = 1;

// Struktur untuk mendefinisikan register yang ingin dibaca
struct RegisterDefinition {
  uint16_t address;
  const char* name;
  const char* dataType;  // "U16", "S16", "U32", "S32", "FLOAT", "U32LE", "S32LE", "FLOATLE"
  const char* jsonKey;   // Key untuk JSON
  float scaleFactor;     // Faktor skala untuk pembagian data
};

// Struktur untuk menyimpan hasil dalam format float
struct ReadingResult {
  const char* name;
  const char* jsonKey;
  float value;
};

// Daftar register yang ingin dibaca
RegisterDefinition registersToRead[] = {
  { 0x0042, "R phase Voltage", "U32", "voltageR", 10000.0 },
  { 0x0044, "S phase Voltage", "U32", "voltageS", 10000.0 },
  { 0x0046, "T phase Voltage", "U32", "voltageT", 10000.0 },
  { 0x0058, "R phase Current", "U32", "currentR", 10000.0 },
  { 0x005A, "S phase Current", "U32", "currentS", 10000.0 },
  { 0x005C, "T phase Current", "U32", "currentT", 10000.0 },
  { 0x0064, "R phase Active Power", "S32", "powerR", 10000.0 },
  { 0x0066, "S phase Active Power", "S32", "powerS", 10000.0 },
  { 0x0068, "T phase Active Power", "S32", "powerT", 10000.0 },
  { 0x006A, "Total Active Power", "S32", "totalActivePower", 10000.0 },
  { 0x006C, "R phase Reactive Power", "S32", "reactivePowerR", 10000.0 },
  { 0x006E, "S phase Reactive Power", "S32", "reactivePowerS", 10000.0 },
  { 0x0070, "T phase Reactive Power", "S32", "reactivePowerT", 10000.0 },
  { 0x0072, "Total Reactive Power", "S32", "totalReactivePower", 10000.0 },
  { 0x0074, "R phase Apparent Power", "S32", "apparentPowerR", 10000.0 },
  { 0x0076, "S phase Apparent Power", "S32", "apparentPowerS", 10000.0 },
  { 0x0078, "T phase Apparent Power", "S32", "apparentPowerT", 10000.0 },
  { 0x007A, "Total Apparent Power", "S32", "totalApparentPower", 10000.0 },
  { 0x007C, "R phase Power Factor", "S16", "powerFactorR", 1000.0 },
  { 0x007D, "S phase Power Factor", "S16", "powerFactorS", 1000.0 },
  { 0x007E, "T phase Power Factor", "S16", "powerFactorT", 1000.0 },
  { 0x007F, "Total Power Factor", "S16", "totalPowerFactor", 1000.0 },
  { 0x0080, "Frequency", "U16", "frequency", 100.0 },
};

const int numRegisters = sizeof(registersToRead) / sizeof(registersToRead[0]);

// Array untuk menyimpan hasil pembacaan dalam format float
ReadingResult readingResults[numRegisters];

void setup() {
  modbusSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(115200);

  // Hubungkan ke WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("✅ Terhubung ke WiFi");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("Fort FCN300 Static Register Reader");
  Serial.println("==================================");
  Serial.println("Membaca register yang telah ditentukan (HEX)");
  Serial.println();

  // Tampilkan daftar register yang akan dibaca
  Serial.println("Register yang akan dibaca:");
  for (int i = 0; i < numRegisters; i++) {
    Serial.print("Reg 0x");
    Serial.print(registersToRead[i].address, HEX);
    Serial.print(" (");
    Serial.print(registersToRead[i].name);
    Serial.print("): ");
    Serial.print(registersToRead[i].dataType);
    Serial.print(", Skala: 1/");
    Serial.println(registersToRead[i].scaleFactor);
  }
  Serial.println();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    baca_registers_statistik();
    kirim_data_ke_appscript();
  } else {
    Serial.println("⚠️ Koneksi WiFi terputus, mencoba reconnect...");
    WiFi.begin(ssid, password);
    delay(5000); // Tunggu 5 detik sebelum mencoba lagi
  }

  delay(30000); // Tunggu 30 detik sebelum membaca lagi
}

void baca_registers_statistik() {
  static uint32_t loopCount = 0;
  loopCount++;

  Serial.print("Pembacaan ke-");
  Serial.println(loopCount);
  Serial.println("----------------------------------");

  for (int i = 0; i < numRegisters; i++) {
    // Dapatkan nilai sebagai float
    float floatValue = getFloatData(registersToRead[i].address, registersToRead[i].dataType);
    
    // Terapkan faktor skala
    float scaledValue = floatValue / registersToRead[i].scaleFactor;

    // Simpan ke array readingResults
    readingResults[i].name = registersToRead[i].name;
    readingResults[i].jsonKey = registersToRead[i].jsonKey;
    readingResults[i].value = scaledValue;

    // Tampilkan hasil
    Serial.print(registersToRead[i].name);
    Serial.print(" (Reg 0x");
    Serial.print(registersToRead[i].address, HEX);

    // Tentukan jumlah register yang perlu dibaca berdasarkan tipe data
    int numRegsToRead = 1;
    if (strcmp(registersToRead[i].dataType, "U32") == 0 || strcmp(registersToRead[i].dataType, "S32") == 0 || strcmp(registersToRead[i].dataType, "FLOAT") == 0 || strcmp(registersToRead[i].dataType, "U32LE") == 0 || strcmp(registersToRead[i].dataType, "S32LE") == 0 || strcmp(registersToRead[i].dataType, "FLOATLE") == 0) {
      numRegsToRead = 2;

      Serial.print("-0x");
      uint16_t endAddr = registersToRead[i].address + 1;
      Serial.print(endAddr, HEX);
    }
    Serial.print("): ");
    Serial.print(floatValue); // Nilai asli dari modbus
    Serial.print(" -> ");
    Serial.print(scaledValue, 6); // Nilai setelah diskala
    Serial.print(" (Skala 1/");
    Serial.print(registersToRead[i].scaleFactor);
    Serial.println(")");
  }

  // Tampilkan isi array readingResults
  Serial.println("\nHasil yang disimpan dalam array:");
  for (int i = 0; i < numRegisters; i++) {
    Serial.print(readingResults[i].name);
    Serial.print(" (");
    Serial.print(readingResults[i].jsonKey);
    Serial.print("): ");
    Serial.println(readingResults[i].value, 6);
  }
}

void kirim_data_ke_appscript() {
  WiFiClientSecure client;
  client.setInsecure();  // disable sertifikat SSL (aman untuk testing)

  HTTPClient http;

  // Mulai koneksi HTTPS
  if (!http.begin(client, GOOGLE_SCRIPT_URL)) {
    Serial.println("❌ Gagal memulai koneksi HTTPS");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  // Buat payload JSON dengan data dari modbus
  DynamicJsonDocument doc(2048);  // Ukuran lebih besar untuk menampung semua data
  
  // Tambahkan timestamp
  doc["timestamp"] = millis();
  
  // Tambahkan semua data dari readingResults (sudah diskala)
  for (int i = 0; i < numRegisters; i++) {
    doc[readingResults[i].jsonKey] = readingResults[i].value;
  }

  String payload;
  serializeJson(doc, payload);

  Serial.print("📤 Mengirim data: ");
  Serial.println(payload);

  // Kirim request POST
  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.printf("✅ Kode respons HTTP: %d\n", httpCode);
    String response = http.getString();
    Serial.println("📥 Respons: " + response);
  } else {
    Serial.printf("❌ Error pada request: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// Fungsi untuk mendapatkan data sebagai float berdasarkan address dan tipe data
float getFloatData(uint16_t address, const char* dataType) {
  // Tentukan jumlah register yang perlu dibaca berdasarkan tipe data
  int numRegsToRead = 1;
  if (strcmp(dataType, "U32") == 0 || strcmp(dataType, "S32") == 0 || strcmp(dataType, "FLOAT") == 0 || strcmp(dataType, "U32LE") == 0 || strcmp(dataType, "S32LE") == 0 || strcmp(dataType, "FLOATLE") == 0) {
    numRegsToRead = 2;
  }

  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(address, numRegsToRead);

  if (result != node.ku8MBSuccess) {
    Serial.print("Error membaca register 0x");
    Serial.print(address, HEX);
    Serial.print(": ");
    printModbusError(result);
    return NAN;
  }

  // Interpretasi berdasarkan tipe data dan konversi ke float
  if (strcmp(dataType, "U16") == 0) {
    return (float)node.getResponseBuffer(0);
  } else if (strcmp(dataType, "S16") == 0) {
    return (float)((int16_t)node.getResponseBuffer(0));
  } else if (strcmp(dataType, "U32") == 0) {
    uint32_t value = ((uint32_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);
    return (float)value;
  } else if (strcmp(dataType, "S32") == 0) {
    int32_t value = ((int32_t)(int16_t)node.getResponseBuffer(0) << 16) | node.getResponseBuffer(1);
    return (float)value;
  } else if (strcmp(dataType, "FLOAT") == 0) {
    return convertToFloat(node.getResponseBuffer(0), node.getResponseBuffer(1));
  } else if (strcmp(dataType, "U32LE") == 0) {
    uint32_t value = ((uint32_t)node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return (float)value;
  } else if (strcmp(dataType, "S32LE") == 0) {
    int32_t value = ((int32_t)(int16_t)node.getResponseBuffer(1) << 16) | node.getResponseBuffer(0);
    return (float)value;
  } else if (strcmp(dataType, "FLOATLE") == 0) {
    return convertToFloat(node.getResponseBuffer(1), node.getResponseBuffer(0));
  }

  Serial.print("Tipe data tidak dikenal: ");
  Serial.println(dataType);
  return NAN;
}

// Fungsi untuk mendapatkan data mentah (raw values) sebagai array
bool getRawData(uint16_t address, int numRegisters, uint16_t* buffer) {
  node.begin(modbusAddr, modbusSerial);
  uint8_t result = node.readInputRegisters(address, numRegisters);

  if (result != node.ku8MBSuccess) {
    return false;
  }

  for (int i = 0; i < numRegisters; i++) {
    buffer[i] = node.getResponseBuffer(i);
  }

  return true;
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
      Serial.print("Unknown Error: ");
      Serial.println(result);
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