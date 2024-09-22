// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
}

// the loop routine runs over and over again forever:
void loop() {
  // read the input on analog pin 0:
  int sensorValue = analogRead(A0);
  // print out the value you read:
  Serial.print("ADC: ");
  Serial.print(sensorValue);
  Serial.print("\tPH: ");
  
  float ph = 26.0708 - ((0.0323125) * (float) sensorValue);
  Serial.println(ph);
  delay(500);  // delay in between reads for stability
}
