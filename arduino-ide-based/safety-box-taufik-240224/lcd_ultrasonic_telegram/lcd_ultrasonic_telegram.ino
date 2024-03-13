
#include <NewPing.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#define BOT_TOKEN "1389983359:AAHGPAMEwdwmO6Bd_PgtU1cvFPNC65r-oXM"
#define CHAT_ID "108488036"

#define TRIGGER_PIN  19  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     18  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
LiquidCrystal_I2C lcd(0x27, 20, 4);  //deklarasi objek lcd
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;

int jarak;
const int batasJarak = 10;

void setup() {
  lcd.init();
  // lcd.begin();
  lcd.backlight();  
  
  Serial.begin(115200); 
  Serial.print("Connecting to WiFi");
  lcdPrompt("Connecting to Wifi ", 0);
  
  WiFi.begin(ssid, pass, 6);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    lcd.print(".");
  }

  lcdPrompt("Connected to :" + String(ssid), 2000);

  Serial.print("Retrieving time: ");
  lcdPrompt("Retrieving time: ", 0);
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
 
  Serial.println(now);
  Serial.println("Connected!");
  lcdPrompt(String(now), 2000);

  lcdPrompt("STANDBY", 1000);
  
  
}

void loop() {
  delay(50);   
  jarak = sonar.ping_cm();                  // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
  Serial.print("Ping: ");
  Serial.print(jarak); // Send ping, get distance in cm and print result (0 = outside set distance range)
  Serial.println("cm");

  if(jarak <= batasJarak){
    lcdPrompt("ADA ORANG", 10);
    kirimNotif("ADA ORANG");
  }else{
    lcdPrompt("Standby", 0);
  }
}

void lcdPrintAll(String baris1, String baris2, String baris3, String baris4, int jeda) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2);
  lcd.setCursor(0, 2);
  lcd.print(baris3);
  lcd.setCursor(0, 3);
  lcd.print(baris4);
  delay(jeda);
}

void lcdPrompt(String prompt, int jeda){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  delay(jeda);
}

void kirimNotif(String teks){
   bot.sendMessage(CHAT_ID, teks, "");
}