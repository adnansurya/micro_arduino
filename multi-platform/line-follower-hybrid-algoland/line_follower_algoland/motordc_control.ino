
void motorDriverSetup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);
}

void arah2Roda(int gerakan, int kecepatan) {
  int pwmKiri = kecepatan * rodaKiri;
  int pwmKanan = kecepatan * rodaKanan;

  switch (gerakan) {
    case 0:  // Stop
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      analogWrite(ENA, 0);
      analogWrite(ENB, 0);
      break;

    case 1:  // Maju
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      analogWrite(ENA, pwmKiri);
      analogWrite(ENB, pwmKanan);
      break;

    case 3:  // Belok kiri
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      analogWrite(ENA, pwmKiri);
      analogWrite(ENB, pwmKanan);
      break;

    case 4:  // Belok kanan
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
      analogWrite(ENA, pwmKiri);
      analogWrite(ENB, pwmKanan);
      break;

    default:
      // Default stop jika gerakan tidak valid
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      analogWrite(ENA, 0);
      analogWrite(ENB, 0);
      break;
  }
}
