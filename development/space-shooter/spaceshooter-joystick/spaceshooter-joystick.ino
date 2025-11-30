// Definisikan pin
const int joyXPin = A0;
const int buttonPin = 2; // D2

void setup() {
  // Inisialisasi komunikasi serial pada baud rate 9600
  Serial.begin(9600); 

  // Set pin tombol sebagai input dengan pull-up internal
  pinMode(buttonPin, INPUT_PULLUP); 
}

void loop() {
  // Baca nilai sumbu X (0-1023)
  int xValue = analogRead(joyXPin); 

  // Baca status tombol (LOW jika ditekan, HIGH jika tidak)
  int buttonState = digitalRead(buttonPin); 

  // Konversi status tombol ke 1 (ditekan) atau 0 (tidak ditekan)
  // Perhatikan INPUT_PULLUP: LOW = ditekan (1), HIGH = tidak ditekan (0)
  int fireStatus = (buttonState == LOW) ? 1 : 0; 

  // Kirim data dalam format terstruktur: "X_VALUE,FIRE_STATUS\n"
  // Contoh: "512,0" atau "1023,1"
  Serial.print(xValue);
  Serial.print(",");
  Serial.println(fireStatus);

  // Jeda singkat untuk menstabilkan komunikasi
  delay(20); 
}