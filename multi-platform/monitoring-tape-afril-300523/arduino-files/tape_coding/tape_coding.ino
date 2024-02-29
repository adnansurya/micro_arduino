
#include <ESP32Servo.h>

#include <WiFi.h>
#include <FirebaseESP32.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#include "DHT.h"

#define DHTPIN 26     // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

#define servoPin 18
#define mq3Pin 33


Servo myservo;  // create servo object to control a servo
// 16 servo objects can be created on the ESP32

int pos = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

//Week Days
String weekDays[7] = { "Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu" };

//Month names
String months[12] = { "Januari", "Februari", "Maret", "April", "Mei", "Juni", "Juli", "Agustus", "September", "Oktober", "November", "Desember" };


#define FIREBASE_HOST "fenta-app-2023-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "iuwZMvm5ibgTuNuQwEI7CQ1g3MRc0ZkFx1ioMuJV"
#define WIFI_SSID "Cafe Tulus"
#define WIFI_PASSWORD "PesanDulu"
//#define WIFI_SSID "Makassar Robotics"
//#define WIFI_PASSWORD "inovasiarduino"

#define GMT_OFFSET 8

FirebaseData firebaseData;
int currentYear = 0;

time_t epochTime;
String weekDay, formattedTime, currentDate, waktu;

DHT dht(DHTPIN, DHTTYPE);

int adcAlkohol = 0;

String kondisi = "-";

long startingTime = 120000;
long batasHari = 3;
//long batasWaktu = batasHari * 86400 * 1000;
long batasWaktu = 300000;


float batasSuhuAtas = 40.0;
float batasSuhuBawah = 35.0;




void setup() {
  Serial.begin(115200);
  
 

  konekWifi();

  setWaktu();
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  
  dht.begin();
  
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);    // standard 50 hz servo
  myservo.attach(servoPin, 1000, 2000);
     myservo.write(120);
      delay(1000);
      myservo.write(0); 
  
  
}

void loop() {

  long sekarang = millis();
  // put your main code here, to run repeatedly:
  getWaktu();
  float t = dht.readTemperature();
   if (isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

 

  adcAlkohol  = analogRead(mq3Pin);
  float persenAlkohol = map(adcAlkohol, 0, 4095, 0, 100);
  Serial.print("MQ : ");
  Serial.println(adcAlkohol);
  delay(1000);

  

  

  if(kondisi == "-"){
    kondisi = "STARTING";
    addHistori(0, 0.0, kondisi);  
  }
  if(sekarang > startingTime){
    //melewati waktu starting

    
    if(sekarang <= batasWaktu){
      //dalam batas waktu fermentasi
      
     if(t > batasSuhuAtas){
      kondisi = "Wadah Terbuka"; 
      myservo.write(120); 
    }else if(t <= batasSuhuAtas && t >= batasSuhuBawah){
      kondisi = "Suhu Optimal";  
      myservo.write(0);
    }else{
      kondisi = "Fermentasi Berlangsung"; 
      myservo.write(0);
     }
      
    }else{
      //melewati batas waktu fermentasi

      if(t > batasSuhuAtas){
         myservo.write(120); 
      }else{
         myservo.write(0); 
      }
  
      if(t > batasSuhuBawah){
        kondisi = "Fermentasi Berhasil";  
      }else{
        kondisi  = "Fermentasi Gagal";  
      }
      
    }     
      
  
  }else{
    return;  
  }
  addHistori(persenAlkohol, t, kondisi);  
 
  delay(15000);
}

void konekWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //memulai menghubungkan ke wifi router

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    //    Serial.print("."); //status saat mengkoneksikan
  }
  Serial.println();

  Serial.print("WiFi Connected to : ");

  Serial.println(WIFI_SSID);

  Serial.print("IP Address : ");

  Serial.println(WiFi.localIP());
  delay(1000);

  //  Serial.println("Sukses terkoneksi wifi!");
  //  Serial.println("IP Address:"); //alamat ip lokal
  //  Serial.println(WiFi.localIP());
}

void addHistori(float alkohol, float suhu, String kondisi) {
  FirebaseJson json;

  json.set("alkohol", alkohol);
  json.set("suhu", suhu);
  json.set("status", kondisi);
  json.set("waktu", waktu);

  if (Firebase.updateNode(firebaseData, "/current", json)) {

    Serial.println(firebaseData.dataPath());

    Serial.println(firebaseData.pushName());

    Serial.println(firebaseData.dataPath() + "/" + firebaseData.pushName());

  } else {
    Serial.println(firebaseData.errorReason());
  }

  if (Firebase.pushJSON(firebaseData, "/histori", json)) {

    Serial.println(firebaseData.dataPath());

    Serial.println(firebaseData.pushName());

    Serial.println(firebaseData.dataPath() + "/" + firebaseData.pushName());

  } else {
    Serial.println(firebaseData.errorReason());
  }
}

void getWaktu() {

  timeClient.update();

  epochTime = timeClient.getEpochTime();

  String tStamp = String(epochTime);

  formattedTime = timeClient.getFormattedTime();

  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentSecond = timeClient.getSeconds();

  weekDay = weekDays[timeClient.getDay()];

  //Get a time structure
  struct tm *ptm = gmtime((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  int currentMonth = ptm->tm_mon + 1;
  String currentMonthName = months[currentMonth - 1];

  currentYear = ptm->tm_year + 1900;

  //Print complete date:
  currentDate = String(monthDay) + "-" + String(currentMonth) + "-" + String(currentYear);

  waktu = weekDay + ", " + currentDate + " " + formattedTime + " WITA";

  Serial.print("->GetWaktu: ");
  Serial.println(waktu);
}

void setWaktu() {

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0

  timeClient.setTimeOffset(GMT_OFFSET * 3600);
}
