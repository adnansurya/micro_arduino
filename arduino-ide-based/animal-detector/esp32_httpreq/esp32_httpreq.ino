#include <WiFi.h>
#include <HTTPClient.h>
#include <NewPing.h>

#define pirPin 34
#define trigPin 32
#define echoPin 33

#define MAX_DISTANCE 200

NewPing sonar(trigPin, echoPin, MAX_DISTANCE); 



// SSID dan password jaringan Wi-Fi
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

String esp32CamIP = "http://192.168.68.79";

String ESP32IP;

int jarak = 0;
const int batasJarak = 15;

int deteksiPir = 0;
int deteksiPing = 0;
int lastPir = 0;
int lastPing = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Memulai IP Scan dengan ESP32");

  pinMode(pirPin, INPUT);

  // Connect ke WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  ESP32IP = WiFi.localIP().toString();  
  Serial.println("\nWiFi Terhubung!");
  Serial.print("IP Address ESP32: ");
  Serial.println(ESP32IP);


}

void loop() {
  // put your main code here, to run repeatedly:
  jarak = (int) sonar.ping_cm();
  delay(50);
  deteksiPir = digitalRead(pirPin);

  Serial.print("Jarak: ");
  Serial.println(jarak);

  if(jarak < batasJarak){
    deteksiPing = 1;
  }else{
    deteksiPing = 0;
  }

 


  if(deteksiPir == 1 && lastPir == 0){
    openURL(esp32CamIP + "/on_pir");
  }

  if(deteksiPing == 1 && lastPing == 0){
     openURL(esp32CamIP + "/on_ping");
  }

  lastPir = deteksiPir;
  lastPing = deteksiPing;

}


void openURL(String urlLink) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(urlLink);

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Response: " + payload);
      // Lakukan sesuatu dengan data yang diterima (misalnya, kendalikan perangkat berdasarkan respons)
    } else {
      Serial.println("Error on HTTP request");
    }

    http.end();
    // delay(5000);  // Tunggu 5 detik sebelum mengirim permintaan berikutnya
  }
}
