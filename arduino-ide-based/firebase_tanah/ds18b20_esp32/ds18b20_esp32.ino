#include <OneWire.h>
#include <DallasTemperature.h>

// Inisialisasi pin data (DQ)
const int oneWireBus = 4; // Gunakan GPIO yang sesuai

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

void setup() {
    Serial.begin(115200);
    sensors.begin();
}

void loop() {
    sensors.requestTemperatures(); // Minta pembacaan suhu
    float temperatureC = sensors.getTempCByIndex(0); // Ambil suhu dalam derajat Celsius
    Serial.print("Suhu: ");
    Serial.print(temperatureC);
    Serial.println(" °C");
    delay(1000); // Tunggu 1 detik sebelum membaca lagi
}
    