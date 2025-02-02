#define PIR_PIN 9
#define ESP32_PIN 8
#define BUZZER_PIN 13

int adaGerak = 0;
int lastGerak = 0;

void setup() {
  
    // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT_PULLUP);
  pinMode(ESP32_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(ESP32_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);


}

void loop() {

  adaGerak = digitalRead(PIR_PIN);

  Serial.print("adaGerak: ");
  Serial.println(adaGerak);

  if(adaGerak){
    digitalWrite(ESP32_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1300);
    digitalWrite(ESP32_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);   
    delay(500); 
  }

  lastGerak = adaGerak;
  delay(200);

}
