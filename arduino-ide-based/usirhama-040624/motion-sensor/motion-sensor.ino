
#define pinMotion 34


int adaGerak;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(pinMotion, INPUT);

}

void loop() {
  // put your main code here, to run repeatedly:

  adaGerak = digitalRead(pinMotion);

  Serial.print("Ada Gerak : ");
  Serial.println(adaGerak);

  delay(1000);

}
