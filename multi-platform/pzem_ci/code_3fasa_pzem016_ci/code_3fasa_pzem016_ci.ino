#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define ssid "MIKRO"
#define password "1DEAlist"

#define relay_R 4
#define relay_S 2
#define relay_T 15

SoftwareSerial pzemSerial(23, 19);  //rx, tx
LiquidCrystal_I2C lcd(0x27, 16, 2);

String serverAddress = "192.168.8.14";
String serverUrl = "http://" + serverAddress + "/save-data";

int batas_arus = 2;
ModbusMaster node;

static uint8_t pzemSlaveAddr1 = 1;
static uint8_t pzemSlaveAddr2 = 2;
static uint8_t pzemSlaveAddr3 = 3;

float voltage, current, power, energy, freq, pf;
String status = "Starting";

void kontrol_relay() {
  if (current >= 2) {
    digitalWrite(relay_R, HIGH);
    digitalWrite(relay_S, HIGH);
    digitalWrite(relay_T, HIGH);
    status = "Off";
  } else {
    digitalWrite(relay_R, LOW);
    digitalWrite(relay_S, LOW);
    digitalWrite(relay_T, LOW);
    status = "On";
  }
}

void setup() {
  pzemSerial.begin(9600);  // baudrate pzem
  Serial.begin(115200);
  pinMode(relay_R, OUTPUT);
  pinMode(relay_S, OUTPUT);
  pinMode(relay_T, OUTPUT);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Monitoring 3Fasa");
  lcd.setCursor(0, 1);
  lcd.print("    Pzem-016     ");
  delay(2000);
  lcd.clear();

  // Connect ke WiFi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  lcd.setCursor(0, 0);
  lcd.print("Mencari WiFi");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi Terhubung!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Terhubung!");
  delay(2000);
}

void loop() {
  baca_pzem(pzemSlaveAddr1, "R");
  baca_pzem(pzemSlaveAddr2, "S");
  baca_pzem(pzemSlaveAddr3, "T");
  kontrol_relay();
}

void baca_pzem(byte addr, String fasa) {
  lcd.clear();
  Serial.print("baca pzem addr: ");
  Serial.println(addr);
  lcd.setCursor(0, 0);
  lcd.print(String() + "Fasa :" + fasa + " -> " + addr);
  delay(1000);
  lcd.clear();

  node.begin(addr, pzemSerial);
  uint8_t result;
  result = node.readInputRegisters(0, 9);  //read the 9 registers of the PZEM-014 / 016
  if (result == node.ku8MBSuccess) {    

    voltage = node.getResponseBuffer(0) / 10.0;
    uint32_t tempdouble = 0x00000000;

    tempdouble = node.getResponseBuffer(1);        //LowByte
    tempdouble |= node.getResponseBuffer(2) << 8;  //highByte
    current = tempdouble / 1000.0;

    tempdouble |= node.getResponseBuffer(3);       //LowByte
    tempdouble |= node.getResponseBuffer(4) << 8;  //highByte
    power = tempdouble / 10.0;

    tempdouble = node.getResponseBuffer(5);        //LowByte
    tempdouble |= node.getResponseBuffer(6) << 8;  //highByte
    energy = tempdouble;

    tempdouble = node.getResponseBuffer(7);
    freq = tempdouble / 10.0;

    tempdouble = node.getResponseBuffer(8);
    pf = tempdouble / 10.0;
    print_data();  
    send_data(fasa, current, voltage, power, freq, pf, energy, status);

  } else {
    Serial.println("Failed to read modbus");
    lcd.setCursor(0, 0);
    lcd.print(String() + "Voltage Tidak ");
    lcd.setCursor(0, 1);
    lcd.print(String() + "Terhubung");
    status = "Disconnected";
    send_data(fasa, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, status);

  }
  
  delay(2000);  //wajib ada delay
}

void send_data(String phase, float curr, float volt, float pow, float freq, float pf, float ener, String stat) {
  String postData = "";
  postData += "phase=";
  postData += phase;
  postData += "&current=";
  postData += String(curr);
  postData += "&voltage=";
  postData += String(volt);
  postData += "&frequency=";
  postData += String(freq);
  postData += "&power=";
  postData += String(pow);
  postData += "&pf=";
  postData += String(pf);
  postData += "&energy=";
  postData += String(ener);
  postData += "&status=";
  postData += stat;

  openURL(postData, serverUrl);
  
}

void print_data() {
  Serial.print(voltage);
  Serial.print("V   ");
  lcd.setCursor(0, 0);
  lcd.print(String() + "V:" + voltage);

  Serial.print(current);
  Serial.print("A   ");
  lcd.setCursor(0, 1);
  lcd.print(String() + "A :" + current);

  Serial.print(power);
  Serial.print("W  ");
  lcd.setCursor(9, 0);
  lcd.print(String() + "P:" + power);

  Serial.print(freq);
  Serial.print("Hz   ");
  lcd.setCursor(9, 1);
  lcd.print(String() + "F:" + freq);

  Serial.print(pf);
  Serial.print("pf   ");
  Serial.print("   ");

  Serial.print(energy);
  Serial.print("Wh  ");
  Serial.println();
}

void openURL(String postStr, String urlLink) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(urlLink);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");


    int httpCode = http.POST(postStr);
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Response: " + payload);
      // Lakukan sesuatu dengan data yang diterima (misalnya, kendalikan perangkat berdasarkan respons)
    } else {
      Serial.println(F("Error on HTTP request"));      
    }

    http.end();
    delay(50); 
  }
}
