#define pirPin 13
#define camPin 15
#define buzzerPin 14

int adaGerak = 0;
int lastGerak = 0;

void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  pinMode(pirPin, INPUT_PULLUP);
  pinMode(camPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  digitalWrite(camPin, LOW);
  digitalWrite(buzzerPin, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  int adaGerak = digitalRead(pirPin);
  Serial.println(adaGerak);
  digitalWrite(camPin, adaGerak);

  if (adaGerak && adaGerak != lastGerak) {
    digitalWrite(buzzerPin, HIGH);
    delay(200);
    digitalWrite(buzzerPin, LOW);
    delay(5000);
  }

  lastGerak = adaGerak;

  delay(10);
}
