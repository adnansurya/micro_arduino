#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "PLAZGOZZ_OUT";
const char* password = "grapari78";

//Your Domain name with URL path or IP address with path
const char* serverName = "http://maulana.bantilang.my.id/postdata.php";


unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

#include "ACS712.h"

#define ANALOG_IN_PIN 34

ACS712 sensor(ACS712_05B, 32);
// Floats for ADC voltage & Input voltage
float adc_voltage = 0.0;
float in_voltage = 0.0;

// Floats for resistor values in divider (in ohms)
float R1 = 30000.0;
float R2 = 7500.0;

// Float for Reference Voltage
float ref_voltage = 3.3;

// Integer for ADC value
int adc_value = 0;



void setup() {
  // Setup Serial Monitor
  Serial.begin(115200);

  sensor.calibrate();

  WiFi.begin(ssid, password);

  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
}

void loop() {

  if ((millis() - lastTime) > timerDelay) {

    // Read the Analog Input
    adc_value = analogRead(ANALOG_IN_PIN);

    // Determine voltage at ADC input
    adc_voltage = (adc_value * ref_voltage) / 4096.0;

    // Calculate voltage at divider input
    in_voltage = adc_voltage / (R2 / (R1 + R2));

    // Print results to Serial Monitor to 2 decimal places
    Serial.print("V = ");
    Serial.print(in_voltage, 2);

    float I = sensor.getCurrentDC();

    // Send it to serial
    Serial.println(String("\tI = ") + I + " A");

    float P = in_voltage * I;
    
    sendData(I, in_voltage, P );
  
    lastTime = millis();
  }

}



void sendData(float arus, float tegangan, float daya) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverName);

    // If you need Node-RED/server authentication, insert user and password below
    //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // Data to send with HTTP POST
    String httpRequestData = "status1=" + String(arus) + "&status2=" + String(tegangan) + "&status3=" + String(daya);
    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);

    // If you need an HTTP request with a content type: application/json, use the following:
    //http.addHeader("Content-Type", "application/json");
    //int httpResponseCode = http.POST("{\"api_key\":\"tPmAT5Ab3j7F9\",\"sensor\":\"BME280\",\"value1\":\"24.25\",\"value2\":\"49.54\",\"value3\":\"1005.14\"}");

    // If you need an HTTP request with a content type: text/plain
    //http.addHeader("Content-Type", "text/plain");
    //int httpResponseCode = http.POST("Hello, World!");

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    // Free resources
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}