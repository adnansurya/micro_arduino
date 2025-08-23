#include <SPI.h>

// Pin definitions untuk dua XC104
const int CS_PIN_1 = 10;  // Chip Select untuk Pot 1
const int CS_PIN_2 = 9;   // Chip Select untuk Pot 2

// Alamat untuk masing-masing potensiometer
#define POT_1 1
#define POT_2 2

void setup() {
  Serial.begin(9600);
  
  // Setup pin mode
  pinMode(CS_PIN_1, OUTPUT);
  pinMode(CS_PIN_2, OUTPUT);
  
  // Deselect kedua chip
  digitalWrite(CS_PIN_1, HIGH);
  digitalWrite(CS_PIN_2, HIGH);
  
  // Initialize SPI
  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  
  Serial.println("Dual XC104 Digital Potentiometer Controller");
  Serial.println("Format command: [POT],[VALUE]");
  Serial.println("Contoh: '1,128' untuk set Pot 1 ke 128");
  Serial.println("         '2,255' untuk set Pot 2 ke 255");
  Serial.println("         '1,0' untuk set Pot 1 ke 0");
}

void loop() {
  // Baca input dari Serial Monitor
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    processCommand(input);
  }
  
  // Demo otomatis (optional)
  // runDemo();
}

void processCommand(String command) {
  command.trim();
  
  // Pisahkan pot number dan value
  int commaIndex = command.indexOf(',');
  
  if (commaIndex == -1) {
    Serial.println("Format salah! Gunakan: [POT],[VALUE]");
    return;
  }
  
  int potNumber = command.substring(0, commaIndex).toInt();
  int value = command.substring(commaIndex + 1).toInt();
  
  // Validasi input
  if (potNumber != 1 && potNumber != 2) {
    Serial.println("Nomor potensiometer harus 1 atau 2");
    return;
  }
  
  if (value < 0 || value > 255) {
    Serial.println("Nilai harus antara 0-255");
    return;
  }
  
  // Set nilai potensiometer
  setPotentiometerValue(potNumber, value);
  
  Serial.print("Pot ");
  Serial.print(potNumber);
  Serial.print(" diatur ke: ");
  Serial.println(value);
}

// Fungsi untuk mengatur nilai potensiometer
void setPotentiometerValue(int potNumber, byte value) {
  int csPin;
  
  // Tentukan CS pin berdasarkan nomor potensiometer
  if (potNumber == POT_1) {
    csPin = CS_PIN_1;
  } else {
    csPin = CS_PIN_2;
  }
  
  // Select chip
  digitalWrite(csPin, LOW);
  
  // Kirim data via SPI
  SPI.transfer(0x00);  // Command byte
  SPI.transfer(value); // Data byte
  
  // Deselect chip
  digitalWrite(csPin, HIGH);
}

// Fungsi untuk set kedua potensiometer sekaligus
void setBothPotentiometers(byte value1, byte value2) {
  setPotentiometerValue(POT_1, value1);
  setPotentiometerValue(POT_2, value2);
  
  Serial.print("Kedua pot diatur - Pot1: ");
  Serial.print(value1);
  Serial.print(", Pot2: ");
  Serial.println(value2);
}

// Demo mode untuk testing
void runDemo() {
  Serial.println("Running demo mode...");
  
  // Demo 1: Kedua pot naik bersama
  for (int i = 0; i <= 255; i += 5) {
    setBothPotentiometers(i, i);
    delay(100);
  }
  
  // Demo 2: Pot 1 turun, Pot 2 naik
  for (int i = 0; i <= 255; i += 5) {
    setBothPotentiometers(255 - i, i);
    delay(100);
  }
  
  // Demo 3: Pattern khusus
  for (int i = 0; i <= 255; i += 10) {
    setPotentiometerValue(POT_1, i);
    setPotentiometerValue(POT_2, 255 - i);
    delay(150);
  }
  
  // Kembali ke nilai tengah
  setBothPotentiometers(128, 128);
  Serial.println("Demo selesai");
}

// Fungsi untuk kontrol dari potentiometer analog
void analogControl() {
  // Baca nilai dari potentiometer analog di pin A0 dan A1
  int analogValue1 = analogRead(A0);
  int analogValue2 = analogRead(A1);
  
  // Konversi ke 8-bit (0-255)
  byte digitalValue1 = map(analogValue1, 0, 1023, 0, 255);
  byte digitalValue2 = map(analogValue2, 0, 1023, 0, 255);
  
  // Set nilai ke digital pots
  setPotentiometerValue(POT_1, digitalValue1);
  setPotentiometerValue(POT_2, digitalValue2);
  
  // Tampilkan nilai setiap 500ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 500) {
    Serial.print("Pot1: ");
    Serial.print(digitalValue1);
    Serial.print(" | Pot2: ");
    Serial.println(digitalValue2);
    lastPrint = millis();
  }
  
  delay(10);
}