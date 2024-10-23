#define BLYNK_TEMPLATE_ID "TMPL6Sok9WCeT"
#define BLYNK_TEMPLATE_NAME "Quickstart Device"
#define BLYNK_AUTH_TOKEN "dfqiV11jtkwE1Z5WbqAiEXZRnvhL51C5"

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


// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "Ninnie";
char pass[] = "nininini";

BlynkTimer timer;
DHT dht(DHTPIN, DHTTYPE);

// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0) {
  // Set incoming value from pin V0 to a variable
  int value = param.asInt();
  dht.begin();

  // Update state
  Blynk.virtualWrite(V1, value);
}

// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED() {
  // Change Web Link Button message to "Congratulations!"
  Blynk.setProperty(V3, "offImageUrl", "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations.png");
  Blynk.setProperty(V3, "onImageUrl", "https://static-image.nyc3.cdn.digitaloceanspaces.com/general/fte/congratulations_pressed.png");
  Blynk.setProperty(V3, "url", "https://docs.blynk.io/en/getting-started/what-do-i-need-to-blynk/how-quickstart-device-was-made");
}

// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent() {
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
   // Read the analog value from the MQ-7 sensor
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



  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));

  Blynk.virtualWrite(V4, h);
  Blynk.virtualWrite(V1, t);
  Blynk.virtualWrite(V2, ppm);
}

void setup() {
  // Debug console
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // You can also specify server:
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "blynk.cloud", 80);
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, IPAddress(192,168,1,100), 8080);

  // Setup a function to be called every second
  analogReadResolution(12);        // Set ADC resolution to 12-bit (0-4095)
  analogSetAttenuation(ADC_11db);  // Set attenuation to measure higher voltage (up to 3.6V)

  timer.setInterval(1000L, myTimerEvent);
}

void loop() {
  Blynk.run();
  timer.run();
  // You can inject your own code or combine it with other sketches.
  // Check other examples on how to communicate with Blynk. Remember
  // to avoid delay() function!
}
