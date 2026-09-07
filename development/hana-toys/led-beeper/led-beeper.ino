#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Deklarasi Pin LED
const int ledMerah  = 2;
const int ledKuning = 3;
const int ledHijau  = 4;

// Deklarasi Pin Buzzer Aktif (Pin PWM)
const int pinBuzzer = 9; 

// Volume Buzzer (0 - 255)
int volumeBuzzer = 40; 

// Total pola dalam urutan
const int TOTAL_POLA = 18;
int indeksPola = 0; // Menunjuk ke pola yang sedang aktif

/* 
  Matriks Pola LED {Merah, Kuning, Hijau}
  HIGH (1) = Nyala | LOW (0) = Mati
*/
const int polaLED[TOTAL_POLA][3] = {
  {LOW,  LOW,  LOW }, // 1. Semua LED mati
  {HIGH, LOW,  LOW }, // 2. Merah only
  {LOW,  HIGH, LOW }, // 3. Kuning only
  {LOW,  LOW,  HIGH}, // 4. Hijau only
  {LOW,  LOW,  LOW }, // 5. Semua LED mati
  {LOW,  LOW,  HIGH}, // 6. Hijau only
  {LOW,  HIGH, LOW }, // 7. Kuning only
  {HIGH, LOW,  LOW }, // 8. Merah only
  {LOW,  LOW,  LOW }, // 9. Semua LED mati
  {HIGH, LOW,  LOW }, // 10. Merah only
  {HIGH, HIGH, LOW }, // 11. Merah kuning
  {HIGH, HIGH, HIGH}, // 12. Semua LED nyala
  {HIGH, HIGH, LOW }, // 13. Merah kuning
  {HIGH, LOW,  LOW }, // 14. Merah only
  {LOW,  LOW,  LOW }, // 15. Semua LED mati
  {LOW,  LOW,  HIGH}, // 16. Hijau only
  {LOW,  HIGH, HIGH}, // 17. Kuning hijau
  {HIGH, HIGH, HIGH}  // 18. Semua LED nyala
};

// Variabel Kontrol Goncangan
float ambangGoncangan = 13.5;   
unsigned long waktuSBL = 0;     
const int jedaGoncangan = 300;  

// Fungsi untuk menerapkan pola LED berdasarkan indeks
void aturPolaLampu(int indeks) {
  digitalWrite(ledMerah,  polaLED[indeks][0]);
  digitalWrite(ledKuning, polaLED[indeks][1]);
  digitalWrite(ledHijau,  polaLED[indeks][2]);
}

// Fungsi beep dengan kontrol volume via PWM
void nadaBeep(int volume) {
  analogWrite(pinBuzzer, volume); 
  delay(50);                      
  analogWrite(pinBuzzer, 0);      
}

void setup() {
  Serial.begin(9600);
  
  pinMode(ledMerah, OUTPUT);
  pinMode(ledKuning, OUTPUT);
  pinMode(ledHijau, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);

  if(!accel.begin()) {
    Serial.println("ADXL345 tidak terdeteksi! Periksa kabel SDA/SCL.");
    while(1);
  }

  accel.setRange(ADXL345_RANGE_4_G);

  // Set pola pertama saat startup & beep pembuka
  aturPolaLampu(indeksPola);
  nadaBeep(volumeBuzzer);
  Serial.println("Sistem Siap! Goncangkan sensor untuk berganti pola LED.");
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);

  float totalAkselerasi = sqrt(event.acceleration.x * event.acceleration.x +
                               event.acceleration.y * event.acceleration.y +
                               event.acceleration.z * event.acceleration.z);

  if (totalAkselerasi > ambangGoncangan && (millis() - waktuSBL > jedaGoncangan)) {
    waktuSBL = millis();
    
    // Pindah ke pola berikutnya (otomatis kembali ke 0 jika sudah mencapai 18)
    indeksPola = (indeksPola + 1) % TOTAL_POLA;
    aturPolaLampu(indeksPola);

    // Beep singkat setiap kali pola berganti
    nadaBeep(volumeBuzzer);

    Serial.print("Goncangan terdeteksi! Nilai: ");
    Serial.print(totalAkselerasi);
    Serial.print(" m/s^2 | Pola Ke-");
    Serial.println(indeksPola + 1); // Menampilkan angka 1 sampai 18 di Serial Monitor
  }
}