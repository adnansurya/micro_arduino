#define pirPin 13
#define camPin 15
#define buzzerPin 14

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

  if(adaGerak){
    digitalWrite(camPin, HIGH);
    delay(50);
    digitalWrite(camPin, LOW);
    digitalWrite(buzzerPin, HIGH);
    delay(2000);
    digitalWrite(buzzerPin, LOW);
    delay(10);
  }

  delay(00);

}
