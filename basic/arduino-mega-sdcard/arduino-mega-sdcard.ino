#include <SPI.h>
#include <SD.h>

// On the Arduino Mega, Pin 53 is the hardware CS/SS pin.
const int chipSelect = 53;

File myFile;

void setup() {
  // Open serial communications
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect (needed for native USB port)
  }

  Serial.print("Initializing SD card...");

  // On the Mega, even if you don't use pin 53 as CS, the hardware SS pin 
  // must remain as an output, or the SPI bus will switch to slave mode.
  if (!SD.begin(chipSelect)) {
    Serial.println("Initialization failed! Check wiring or card format.");
    return;
  }
  Serial.println("Initialization done.");

  // --- WRITE FILE ---
  // Open the file. Note: only one file can be open at a time.
  myFile = SD.open("test.txt", FILE_WRITE);

  if (myFile) {
    Serial.print("Writing to test.txt...");
    myFile.println("Hello, Arduino Mega!");
    myFile.close(); // Always close the file to save data!
    Serial.println("Done.");
  } else {
    Serial.println("Error opening test.txt for writing.");
  }

  // --- READ FILE ---
  myFile = SD.open("absensi.csv");
  if (myFile) {
    Serial.println("Contents of test.txt:");

    // Read from the file until there's nothing else in it:
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    myFile.close();
  } else {
    Serial.println("Error opening test.txt for reading.");
  }
}

void loop() {
  // Nothing here for this setup demo
}