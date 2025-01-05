#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <MAX30100_PulseOximeter.h>
#include <Adafruit_MPU6050.h>
#include <LiquidCrystal_I2C.h>

#define REPORTING_PERIOD_MS 1000

// Inisialisasi sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();  // MLX90614
PulseOximeter pox;                            // MAX30100
Adafruit_MPU6050 mpu;                         // MPU6050
LiquidCrystal_I2C lcd(0x27, 16, 2);           // Alamat I2C untuk LCD (0x27 untuk banyak modul LCD I2C)

// Variabel untuk menyimpan data
float suhuTubuh;
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

  // Inisialisasi MLX90614
  if (!mlx.begin()) {
    Serial.println("Gagal menginisialisasi MLX90614!");
    while (1)
      ;
  }
  Serial.println("MLX90614 berhasil diinisialisasi.");

  // Inisialisasi MAX30100


  // Inisialisasi MPU6050
  if (!mpu.begin()) {
    Serial.println("Gagal menginisialisasi MPU6050!");
    while (1)
      ;
  }
  Serial.println("MPU6050 berhasil diinisialisasi.");

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Inisialisasi...");
  delay(2000);
  lcd.clear();

  if (!pox.begin()) {
    Serial.println("Gagal menginisialisasi MAX30100!");
    while (1)
      ;
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_24MA);
  // pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onPulseOximeterData);
  Serial.println("MAX30100 berhasil diinisialisasi.");

  Serial.println("Semua sensor berhasil diinisialisasi.\n");
}

void loop() {

  pox.update();
  // Membaca data dari MLX90614 (Suhu Tubuh)


  // Membaca data dari MAX30100 (Detak Jantung dan SpO2)

  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    heartRate = pox.getHeartRate();
    spO2 = pox.getSpO2();
    Serial.print("Detak Jantung: ");
    Serial.print(heartRate);
    Serial.print(" bpm | Saturasi Oksigen: ");
    Serial.print(spO2);
    Serial.println(" %");

    lcd.setCursor(0, 1);
    lcd.print("HR:");
    if (heartRate > 0) {
      lcd.print(heartRate, 0);  // Menampilkan detak jantung tanpa desimal
    } else {
      lcd.print("--");
    }
    lcd.print("bpm ");
    lcd.print("O2:");
    if (spO2 > 0) {
      lcd.print(spO2, 0);  // Menampilkan saturasi oksigen tanpa desimal
      lcd.print("%");
    } else {
      lcd.print("--%");
    }

    suhuTubuh = mlx.readObjectTempC();
    if (isnan(suhuTubuh)) {
      Serial.println("Gagal membaca data suhu tubuh dari MLX90614.");
      lcd.setCursor(0, 0);
      lcd.print("Temp: Error");
    } else {
      Serial.print("Suhu Tubuh: ");
      Serial.print(suhuTubuh);
      Serial.println(" °C");
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(suhuTubuh, 1);  // Menampilkan suhu dengan 1 angka desimal
      lcd.print(" C");
    }

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
