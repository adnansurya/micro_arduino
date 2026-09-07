#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Deklarasi Pin LED
const int ledMerah  = 2;
const int ledKuning = 3;
const int ledHijau  = 4;

// Deklarasi Pin Buzzer Aktif (Harus Pin PWM: 3, 5, 6, 9, 10, atau 11)
const int pinBuzzer = 9; 

// ATUR VOLUME BUZZER AKTIF DI SINI (Nilai PWM: 0 hingga 255)
// 255 = Volume Maksimal (100%)
// 50  = Volume Sedang/Pelan (~20%)
// 10  = Volume Sangat Pelan (~4%)
int volumeBuzzer = 40; 

// Variabel Kontrol Goncangan
int modeLampu = 0;              
float ambangGoncangan = 15.0;   
unsigned long waktuSBL = 0;     
const int jedaGoncangan = 300;  

void aturLampu(int mode) {
  digitalWrite(ledMerah,  mode == 0 ? HIGH : LOW);
  digitalWrite(ledKuning, mode == 1 ? HIGH : LOW);
  digitalWrite(ledHijau,  mode == 2 ? HIGH : LOW);
}

// Fungsi beep dengan kontrol volume via PWM
void nadaBeep(int volume) {
  analogWrite(pinBuzzer, volume); // Mengirim sinyal PWM untuk mengatur volume
  delay(50);                      // Durasi beep
  analogWrite(pinBuzzer, 0);      // Matikan buzzer (PWM = 0)
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

  // Lampu awal & beep pembuka
  aturLampu(modeLampu);
  nadaBeep(volumeBuzzer);
  Serial.println("Sistem Siap! Goncangkan sensor untuk berganti lampu.");
}

void loop() {
  sensors_event_t event;
  accel.getEvent(&event);

  float totalAkselerasi = sqrt(event.acceleration.x * event.acceleration.x +
                               event.acceleration.y * event.acceleration.y +
                               event.acceleration.z * event.acceleration.z);

  if (totalAkselerasi > ambangGoncangan && (millis() - waktuSBL > jedaGoncangan)) {
    waktuSBL = millis();
    
    // Berganti mode lampu
    modeLampu = (modeLampu + 1) % 3;
    aturLampu(modeLampu);

    // Beep singkat dengan volume yang ditentukan
    nadaBeep(volumeBuzzer);

    Serial.print("Goncangan terdeteksi! Nilai: ");
    Serial.print(totalAkselerasi);
    Serial.print(" m/s^2 | Lampu: ");
    if (modeLampu == 0) Serial.println("MERAH");
    else if (modeLampu == 1) Serial.println("KUNING");
    else Serial.println("HIJAU");
  }
}