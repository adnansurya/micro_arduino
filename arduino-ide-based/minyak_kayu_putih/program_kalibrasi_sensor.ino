// TCS3200 Pins
const int S0 = 4;
const int S1 = 5;
const int S2 = 6;
const int S3 = 7;
const int sensorOut = 8;

unsigned int redFrequency, greenFrequency, blueFrequency; // Variabel untuk nilai warna

void setup() {
  // Inisialisasi pin sebagai output atau input
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(sensorOut, INPUT);
  
  // Atur kecepatan frekuensi scaling ke 20%
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);

  // Inisialisasi Serial Monitor
  Serial.begin(9600);
}

void loop() {
  // Baca nilai merah
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  redFrequency = pulseIn(sensorOut, LOW);

  // Baca nilai hijau
  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  greenFrequency = pulseIn(sensorOut, LOW);

  // Baca nilai biru
  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  blueFrequency = pulseIn(sensorOut, LOW);

  // Tampilkan nilai warna di Serial Monitor
  Serial.print("Merah: ");
  Serial.print(redFrequency);
  Serial.print(" Hijau: ");
  Serial.print(greenFrequency);
  Serial.print(" Biru: ");
  Serial.println(blueFrequency);

  delay(1000); // Delay 1 detik sebelum membaca nilai lagi
}
