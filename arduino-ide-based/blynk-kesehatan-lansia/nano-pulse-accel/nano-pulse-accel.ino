#include <Wire.h>
#include <MAX30100_PulseOximeter.h>
#include <Adafruit_MPU6050.h>

#define REPORTING_PERIOD_MS 1000

PulseOximeter pox;     // MAX30100
Adafruit_MPU6050 mpu;  // MPU6050

// Variabel untuk menyimpan data
float heartRate, spO2;
float accelX, accelY, accelZ;  // Data akselerasi

uint32_t tsLastReport = 0;

// Callback untuk pembaruan data MAX30100
void onPulseOximeterData() {
  Serial.println("BEAT");
}

void setup() {
  // Inisialisasi Serial Monitor
  Serial.begin(115200);
  Serial.println("Inisialisasi sensor...");

  // Inisialisasi MPU6050
  if (!mpu.begin()) {
    Serial.println("Gagal menginisialisasi MPU6050!");
    while (1)
      ;
  }
  Serial.println("MPU6050 berhasil diinisialisasi.");

  // Inisialisasi MAX30100
  if (!pox.begin()) {
    Serial.println("Gagal menginisialisasi MAX30100!");
    while (1)
      ;
  }
  // pox.setIRLedCurrent(MAX30100_LED_CURR_24MA);
  pox.setIRLedCurrent(MAX30100_LED_CURR_27_1MA);
  pox.setOnBeatDetectedCallback(onPulseOximeterData);
  Serial.println("MAX30100 berhasil diinisialisasi.");

  Serial.println("Semua sensor berhasil diinisialisasi.\n");
}

void loop() {

  pox.update();

  // Membaca data dari MAX30100 (Detak Jantung dan SpO2)
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    heartRate = pox.getHeartRate();
    spO2 = pox.getSpO2();
    Serial.print("Detak Jantung: ");
    Serial.print(heartRate);
    Serial.print(" bpm | Saturasi Oksigen: ");
    Serial.print(spO2);
    Serial.println(" %");

    

    // Membaca data dari MPU6050 (Akselerasi)
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    accelX = accel.acceleration.x;
    accelY = accel.acceleration.y;
    accelZ = accel.acceleration.z;

    Serial.print("Akselerasi (m/s²) - X: ");
    Serial.print(accelX);
    Serial.print(", Y: ");
    Serial.print(accelY);
    Serial.print(", Z: ");
    Serial.println(accelZ);

    Serial.println("----------------------------");

    tsLastReport = millis();
  }
}
