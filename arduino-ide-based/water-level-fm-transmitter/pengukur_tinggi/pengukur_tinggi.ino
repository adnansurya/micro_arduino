#include <NewPing.h>

#define TRIGGER_PIN  26  
#define ECHO_PIN     25  
#define MAX_DISTANCE 200 

long tinggiMax = 150;
long jarakUkur, tinggiAir;
long batasTinggi = 30;


NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); 


void setup() {
  Serial.begin(115200); 
}

void loop() {
  delay(500);  
  jarakUkur = sonar.ping_cm();         

  tinggiAir = tinggiMax - jarakUkur;
  
  if(tinggiAir > 30){
    Serial.println("BAHAYA!");
  }

  Serial.print("Jarak Ukur: ");
  Serial.print(jarakUkur); 
  Serial.print(" cm\t");
  Serial.print("Tinggi Air : ");
  Serial.print(tinggiAir);
  Serial.println("cm");


}