int pinLed = 13;
int pinSensor = 8;
int pinRelay = 11;
int pinRelay2 = 12;

void setup() {
  // put your setup code here, to run once:
  pinMode(pinRelay, OUTPUT);
  pinMode(pinRelay2, OUTPUT);
  pinMode(pinLed, OUTPUT);
  pinMode(pinSensor, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  int basah = digitalRead(pinSensor);
  digitalWrite(pinRelay, basah);
  digitalWrite(pinLed, basah);
  digitalWrite(pinRelay2, !basah);
}
