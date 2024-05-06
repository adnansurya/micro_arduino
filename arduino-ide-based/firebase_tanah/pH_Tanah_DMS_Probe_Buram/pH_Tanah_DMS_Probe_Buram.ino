/**********************************
 * Compile using Arduino IDE 1.8.15
 * Board ESP32 Dev Module 
 * Using DMS
 **********************************/

//input output
#define DMSpin  13     // pin output untuk DMS
#define indikator  2   // pin output untuk indikator pembacaan sensor
#define adcPin 34      // pin input sensor pH tanah

//variabel     
int ADC;
float lastReading;
float pH;

void setup() {

  Serial.begin(115200);          // setting baudrate komunikasi serial
  analogReadResolution(10);      // setting resolusi pembacaan ADC menjadi 10 bit
  pinMode(DMSpin, OUTPUT);
  pinMode(indikator, OUTPUT);
  digitalWrite(DMSpin,HIGH);     // non-aktifkan DMS
}

void loop() {
  
  digitalWrite(DMSpin,LOW);      // aktifkan DMS
  digitalWrite(indikator, HIGH); // led indikator built-in ESP32 menyala
  delay(10*1000);                // wait DMS capture data
  ADC = analogRead(adcPin); 

/***************************************************************
 *  Setelah anda melakukan proses kalibrasi dengan benar 
 *  dan mencari rumus kalibrasi menggunakan regresi linier excel
 *  maka nilai rumus regresi dibawah ini gantilah dengan nilai
 *  rumus regresi yang anda dapatkan
 ***************************************************************/
  
  pH  = (-0.0139*ADC) + 7.7851;  // ini adalah rumus regresi linier yang wajib anda ganti!
    if (pH != lastReading) { 
    lastReading = pH; 
    }
  Serial.print("ADC=");
  Serial.print(ADC);             // menampilkan nilai ADC di serial monitor pada baudrate 115200
  Serial.print(" pH=");
  Serial.println(lastReading,1); // menampilkan nilai pH di serial monitor pada baudrate 115200

  digitalWrite(DMSpin,HIGH);
  digitalWrite(indikator,LOW);
  delay(3*1000);                 // wait for DMS ready
}
