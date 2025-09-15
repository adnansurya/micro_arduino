#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <BluetoothSerial.h>

SoftwareSerial pzemSerial(23, 19);  // rx, tx untuk PZEM
BluetoothSerial SerialBT;           // Bluetooth Serial

ModbusMaster node;

static uint8_t pzemSlaveAddr1 = 1;
static uint8_t pzemSlaveAddr2 = 2;
static uint8_t pzemSlaveAddr3 = 3;

float voltage, current, power, energy, freq, pf;

void setup() {
  pzemSerial.begin(9600);  // baudrate pzem
  Serial.begin(115200);
  SerialBT.begin("ESP32-PZEM-3Fase"); // Nama perangkat Bluetooth
  
  Serial.println("Monitoring Listrik 3 Fasa PZEM-016");
  Serial.println("==================================");
  SerialBT.println("Monitoring Listrik 3 Fasa PZEM-016");
  SerialBT.println("==================================");
}

void loop() {
  baca_pzem(pzemSlaveAddr1, "R");
  baca_pzem(pzemSlaveAddr2, "S");
  baca_pzem(pzemSlaveAddr3, "T");
  
  Serial.println("==================================");
  SerialBT.println("==================================");
  delay(2000);
}

void baca_pzem(byte addr, String fasa) {
  Serial.print("Membaca PZEM addr: ");
  Serial.print(addr);
  Serial.print(" (Fasa ");
  Serial.print(fasa);
  Serial.println(")");
  
  SerialBT.print("Membaca PZEM addr: ");
  SerialBT.print(addr);
  SerialBT.print(" (Fasa ");
  SerialBT.print(fasa);
  SerialBT.println(")");

  node.begin(addr, pzemSerial);
  uint8_t result;
  result = node.readInputRegisters(0, 9);  // baca 9 register dari PZEM-014/016
  
  if (result == node.ku8MBSuccess) {    
    voltage = node.getResponseBuffer(0) / 10.0;
    
    uint32_t tempdouble = 0x00000000;
    tempdouble = node.getResponseBuffer(1);        // LowByte
    tempdouble |= node.getResponseBuffer(2) << 8;  // HighByte
    current = tempdouble / 1000.0;

    tempdouble = node.getResponseBuffer(3);        // LowByte
    tempdouble |= node.getResponseBuffer(4) << 8;  // HighByte
    power = tempdouble / 10.0;

    tempdouble = node.getResponseBuffer(5);        // LowByte
    tempdouble |= node.getResponseBuffer(6) << 8;  // HighByte
    energy = tempdouble;

    tempdouble = node.getResponseBuffer(7);
    freq = tempdouble / 10.0;

    tempdouble = node.getResponseBuffer(8);
    pf = tempdouble / 10.0;
    
    print_data(fasa);
    
  } else {
    Serial.println("Gagal membaca modbus");
    SerialBT.println("Gagal membaca modbus");
    Serial.println();
    SerialBT.println();
  }
  
  delay(1000);
}

void print_data(String fasa) {
  // Tampilkan di Serial Monitor
  Serial.print("Fasa ");
  Serial.print(fasa);
  Serial.print(": ");
  Serial.print(voltage);
  Serial.print("V, ");
  Serial.print(current);
  Serial.print("A, ");
  Serial.print(power);
  Serial.print("W, ");
  Serial.print(freq);
  Serial.print("Hz, ");
  Serial.print(pf);
  Serial.print("pf, ");
  Serial.print(energy);
  Serial.println("Wh");

  // Tampilkan di Bluetooth
  SerialBT.print("Fasa ");
  SerialBT.print(fasa);
  SerialBT.print(": ");
  SerialBT.print(voltage);
  SerialBT.print("V, ");
  SerialBT.print(current);
  SerialBT.print("A, ");
  SerialBT.print(power);
  SerialBT.print("W, ");
  SerialBT.print(freq);
  SerialBT.print("Hz, ");
  SerialBT.print(pf);
  SerialBT.print("pf, ");
  SerialBT.print(energy);
  SerialBT.println("Wh");
}

void print_data_formatted(String fasa) {
  // Format yang lebih rapi untuk Bluetooth
  SerialBT.println("================================");
  SerialBT.print("FASA ");
  SerialBT.println(fasa);
  SerialBT.println("================================");
  SerialBT.print("Tegangan: ");
  SerialBT.print(voltage);
  SerialBT.println(" V");
  SerialBT.print("Arus:     ");
  SerialBT.print(current);
  SerialBT.println(" A");
  SerialBT.print("Daya:     ");
  SerialBT.print(power);
  SerialBT.println(" W");
  SerialBT.print("Frekuensi:");
  SerialBT.print(freq);
  SerialBT.println(" Hz");
  SerialBT.print("PF:       ");
  SerialBT.print(pf);
  SerialBT.println("");
  SerialBT.print("Energi:   ");
  SerialBT.print(energy);
  SerialBT.println(" Wh");
  SerialBT.println();
}