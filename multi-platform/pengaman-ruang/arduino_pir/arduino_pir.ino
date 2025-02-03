#define PIR_PIN 12
#define ESP32_PIN 11
#define LED_PIN 13

int adaGerak = 0;

void setup() {

  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT_PULLUP);
  pinMode(ESP32_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(ESP32_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

void loop() {

  adaGerak = digitalRead(PIR_PIN);

  Serial.print("adaGerak: ");
  Serial.println(adaGerak);

  if (adaGerak) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(ESP32_PIN, HIGH);

  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(ESP32_PIN, LOW);
  }

  delay(200);
}
