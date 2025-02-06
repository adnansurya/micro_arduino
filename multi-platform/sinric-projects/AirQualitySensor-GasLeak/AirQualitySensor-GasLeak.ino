/*
 * Example for how to use SinricPro Air Quality Sensor device
 * 
 * If you encounter any issues:
 * - check the readme.md at https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md
 * - ensure all dependent libraries are installed
 *   - see https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md#arduinoide
 *   - see https://github.com/sinricpro/esp8266-esp32-sdk/blob/master/README.md#dependencies
 * - open serial monitor and check whats happening
 * - check full user documentation at https://sinricpro.github.io/esp8266-esp32-sdk
 * - visit https://github.com/sinricpro/esp8266-esp32-sdk/issues and check for existing issues or open a new one
 */
 
// Uncomment the following line to enable serial debug output
//#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
  #define DEBUG_ESP_PORT Serial
  #define NODEBUG_WEBSOCKETS
  #define NDEBUG
#endif 


#include <Arduino.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
  #include <WiFi.h>
#endif

#include "SinricPro.h"
#include "SinricProAirQualitySensor.h"

#define WIFI_SSID       "MIKRO"    
#define WIFI_PASS       "1DEAlist"
#define APP_KEY    "ccb91012-9e08-4a49-82c7-087768d4734a"
#define APP_SECRET "fde1ae39-cf8f-46c1-8fbf-1e4f289f691a-e53009f6-4880-4216-b0dd-a9e05f7654db"
#define DEVICE_ID  "67a36bb1cc6fea0ed3fd1228"
#define BAUD_RATE         115200 // Change baudrate to your need

// Air quality sensor event dispatch time.  Min is every 1 min. 
#define MIN (1000UL * 60 * 1)
unsigned long dispatchTime = millis() + MIN;
 
 // setup function for WiFi connection
void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");

  #if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP); 
    WiFi.setAutoReconnect(true);
  #elif defined(ESP32)
    WiFi.setSleep(false); 
    WiFi.setAutoReconnect(true);
  #endif

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
}

// setup function for SinricPro
void setupSinricPro() {
  // add device to SinricPro
  SinricProAirQualitySensor& mySinricProAirQualitySensor = SinricPro[DEVICE_ID];

  // set callback function to device
 
  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
} 

void setup() {
  Serial.begin(BAUD_RATE); Serial.printf("\r\n\r\n");
  setupWiFi();
  setupSinricPro();  
}

void loop() {
  SinricPro.handle();

  if((long)(millis() - dispatchTime) >= 0) {
    SinricProAirQualitySensor &mySinricProAirQualitySensor = SinricPro[DEVICE_ID]; // get sensor device
    
    int pm1 =0;
    int pm2_5 = 0;   
    int pm10=0;   

    pm2_5 = random(500);
    pm1 = random(500);
    pm10 = random(500);

    Serial.print(pm2_5);
    Serial.print(" | ");
    Serial.print(pm1);
    Serial.print(" | ");
    Serial.println(pm10);    
    
    bool success = mySinricProAirQualitySensor.sendAirQualityEvent(pm1, pm2_5, pm10, "PERIODIC_POLL");
    if(success) {
      Serial.println("Air Quality event sent! ..");
    } else {
      Serial.printf("Something went wrong...could not send Event to server!\r\n");
    }

    dispatchTime += MIN;
  }  
}