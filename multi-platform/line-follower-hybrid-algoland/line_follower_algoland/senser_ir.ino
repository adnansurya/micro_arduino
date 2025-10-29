void SensorInfrared() {
  kiri = analogRead(sensorKiri);
  kanan = analogRead(sensorKanan);
}

void SensorInfraredSetup() {
  pinMode(sensorKiri, INPUT);
  pinMode(sensorKanan, INPUT);
}

void kalibrasiSensorOtomatis() {
  unsigned long startTime = millis();  // Waktu mulai kalibrasi
  unsigned long calibrationTime = 5000;  // Durasi kalibrasi: 5 detik

  Serial.println("Kalibrasi dimulai... Silakan gerakkan robot di atas garis hitam dan putih selama 5 detik.");
  digitalWrite(ledPin, HIGH);  // Nyalakan LED selama kalibrasi

  while (millis() - startTime < calibrationTime) {
    // Baca nilai sensor
    kiri = analogRead(sensorKiri);
    kanan = analogRead(sensorKanan);

    // Update nilai minimum dan maksimum untuk sensor kiri
    if (kiri < kiriMin) kiriMin = kiri;
    if (kiri > kiriMax) kiriMax = kiri;

    // Update nilai minimum dan maksimum untuk sensor kanan
    if (kanan < kananMin) kananMin = kanan;
    if (kanan > kananMax) kananMax = kanan;

    delay(10);  // Jeda singkat untuk menghindari pembacaan terlalu cepat
  }

  digitalWrite(ledPin, LOW);  // Matikan LED setelah kalibrasi selesai

  // Hitung threshold untuk sensor kiri dan kanan
  kiriThreshold = (kiriMin + kiriMax) / 2;
  kananThreshold = (kananMin + kananMax) / 2;

  Serial.print("Nilai Minimum Kiri: ");
  Serial.println(kiriMin);
  Serial.print("Nilai Maksimum Kiri: ");
  Serial.println(kiriMax);
  Serial.print("Threshold Kiri: ");
  Serial.println(kiriThreshold);

  Serial.print("Nilai Minimum Kanan: ");
  Serial.println(kananMin);
  Serial.print("Nilai Maksimum Kanan: ");
  Serial.println(kananMax);
  Serial.print("Threshold Kanan: ");
  Serial.println(kananThreshold);
}