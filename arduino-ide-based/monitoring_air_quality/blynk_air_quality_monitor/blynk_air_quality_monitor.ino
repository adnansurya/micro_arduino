#define BLYNK_TEMPLATE_ID "TMPL6Sok9WCeT"
#define BLYNK_TEMPLATE_NAME "Quickstart Device"
#define BLYNK_AUTH_TOKEN "dfqiV11jtkwE1Z5WbqAiEXZRnvhL51C5"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial


#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "DHT.h"

#define DHTPIN 16
#define DHTTYPE DHT11

// Pin where the MQ-7 sensor's analog output is connected
const int mq7Pin = 34;  // GPIO34 for ADC1_CH6

// Parameters for calibration (based on the sensor's datasheet or your setup)
const float RLOAD = 10.0;       // Load resistance on the sensor (Ohms)
const float RZERO = 10.0;       // Sensor resistance in fresh air (Ohms)
const float PPM_SCALING = 1.0;  // Scaling factor for converting the analog signal to PPM (adjust as necessary)

// Variables to store sensor values
int rawValue = 0;
float sensorVoltage = 0.0;
float rs = 0.0;  // Resistance of the sensor
float ppm = 0.0;

int displayIter = 0;
String status = "-";


// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "Ninnie";
char pass[] = "nininini";

// char ssid[] = "MIKRO";
// char pass[] = "1DEAlist";

float h, t, lastH, lastT;
int mapH;

int badPoints = 0;
int lastBadPoints = 0;

BlynkTimer timer;
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0) {
  // Set incoming value from pin V0 to a variable
  int value = param.asInt();
  dht.begin();

  // Update state
  Blynk.virtualWrite(V1, value);
}


void updateData() {
  rawValue = analogRead(mq7Pin);

  // Convert the raw value to voltage (for 12-bit resolution, 0-4095 = 0-3.3V)
  sensorVoltage = rawValue * (3.3 / 4095.0);

  // Calculate the resistance of the sensor (Rs)
  rs = (3.3 - sensorVoltage) / sensorVoltage * RLOAD;

  // Estimate CO concentration in PPM (parts per million)
  // You can apply a formula from the MQ-7 sensor datasheet or a simplified model
  ppm = (rs / RZERO) * PPM_SCALING;  // Adjust the scaling factor for calibration

  // Print the results
  Serial.print("Raw Value: ");
  Serial.print(rawValue);
  Serial.print(" | Voltage: ");
  Serial.print(sensorVoltage);
  Serial.print(" V | Rs: ");
  Serial.print(rs);
  Serial.print(" Ohms | CO Concentration: ");
  Serial.print(ppm);
  Serial.println(" PPM");

  h = dht.readHumidity();

  mapH = map(h, 50, 100, 0, 100);

  t = dht.readTemperature() - 3.0;

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || mapH > 100 || mapH < 0) {
    Serial.println(F("Failed to read from DHT sensor!"));
    mapH = lastH;
    t = lastT;
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));


  lastH = mapH;
  lastT = t;
}




// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent() {


  lcd.clear();
  if (displayIter == 0) {

    lcd.setCursor(0, 0);
    lcd.print("Suhu: ");
    lcd.print(t);
    lcd.print(" ");
    lcd.print((char)223);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("CO  : ");
    lcd.print(ppm);
    lcd.print(" ppm");


  } else if (displayIter == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Lembab : ");
    lcd.print(mapH);
    lcd.print(" %");
  } else {
    lcd.setCursor(1, 0);
    lcd.print("Kualitas Udara");
    lcd.setCursor(5, 1);

    if (ppm >= 35.0) {
      badPoints++;
    }
    if (mapH > 70 || mapH < 30) {
      badPoints++;
    }

    if (t < 18 || t > 30) {
      badPoints++;
    }


    if (badPoints == 0) {
      lcd.print("Ideal");
      status = "Ideal";
    } else if (badPoints == 1) {
      lcd.print("Sedang");
      status = "Sedang";
    } else {
      lcd.print("Buruk");
      status = "Buruk";
      if(badPoints != lastBadPoints){
        Blynk.logEvent("Peringatan!", "Kualitas Udara Buruk!") ;
      }
    }

    lastBadPoints = badPoints;
    badPoints = 0;
  }



  if (displayIter >= 2) {
    displayIter = 0;
  } else {
    displayIter++;
  }


  Blynk.virtualWrite(V4, mapH);
  Blynk.virtualWrite(V1, t);
  Blynk.virtualWrite(V2, ppm);
  Blynk.virtualWrite(V3, "Kualitas Udara " + status );
}

void setup() {
  // Debug console
  Serial.begin(115200);
  lcd.init();

  lcd.backlight();
  lcd.setCursor(1, 0);
  lcd.print("Water Quality");
  lcd.setCursor(4, 1);
  lcd.print("Monitor");

  delay(4000);

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Components by");
  lcd.setCursor(0, 1);
  lcd.print("MakassarRobotics");

  delay(1500);


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to...");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  delay(1000);
  // You can also specify server:
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "blynk.cloud", 80);
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, IPAddress(192,168,1,100), 8080);

  // Setup a function to be called every second
  analogReadResolution(12);        // Set ADC resolution to 12-bit (0-4095)
  analogSetAttenuation(ADC_11db);  // Set attenuation to measure higher voltage (up to 3.6V)


  timer.setInterval(1000L, updateData);
  timer.setInterval(2000L, myTimerEvent);

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("Blynk IoT App");
  lcd.setCursor(3, 1);
  lcd.print("Connected!");
  delay(2000);
}

void loop() {
  Blynk.run();
  timer.run();
  // You can inject your own code or combine it with other sketches.
  // Check other examples on how to communicate with Blynk. Remember
  // to avoid delay() function!
}
