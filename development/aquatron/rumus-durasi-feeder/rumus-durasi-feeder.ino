// C++ code
//

float durasiAlat(int day, int totalIkan){

  const float fr = 0.05;
  const float debitAlat = 0.06;

  float beratPerEkor = 0.0;

  
  if(day >= 5){
    beratPerEkor = 0.0003;
  	for(int i=0; i<day-4; i++){
      if(i == 0){
        continue;
      }else{
          beratPerEkor = beratPerEkor * 1.0409;

      }
    }
  }

  Serial.print("Berat per Ekor: ");
  Serial.println(beratPerEkor, 16);

  float biomassa = totalIkan * beratPerEkor;
  float pakanPerHari = biomassa * fr;
  float porsi = pakanPerHari / 2.0;
  float durasi = porsi / debitAlat;

  Serial.print("Biomassa: ");
  Serial.println(biomassa, 16);
  Serial.print("Pakan per hari: ");
  Serial.println(pakanPerHari, 16);
  Serial.print("Porsi: ");
  Serial.println(porsi, 16);
  Serial.print("Durasi: ");
  Serial.println(durasi, 18);

  
 return durasi;

}

void setup()
{
  Serial.begin(9600);
  
  int hari = 15;
  int jumlahIkan = 30;
  float durasims = durasiAlat(hari, jumlahIkan);
  
}

void loop()
{
  
}