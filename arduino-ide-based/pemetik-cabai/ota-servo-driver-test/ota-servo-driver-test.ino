#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo config
#define SERVOMIN  150
#define SERVOMAX  600
#define USMIN     600
#define USMAX     2400
#define SERVO_FREQ 50

// Servo counter
uint8_t servonum = 0;

// WiFi credentials (Access Point)
const char* ssid = "ESP32_AP";
const char* password = "12345678";

// Web server
AsyncWebServer server(80);

// HTML OTA update form
const char* updatePage = R"rawliteral(
<!DOCTYPE html><html>
  <head><title>ESP32 OTA</title></head>
  <body>
    <h2>ESP32 OTA Update</h2>
    <form method='POST' action='/update' enctype='multipart/form-data'>
      <input type='file' name='update'>
      <input type='submit' value='Upload'>
    </form>
    <p><a href="/">Back to Home</a></p>
  </body>
</html>
)rawliteral";

// Home page
const char* homePage = R"rawliteral(
<!DOCTYPE html><html>
  <head><title>ESP32 Home</title></head>
  <body>
    <h2>ESP32 PCA9685 Servo Controller</h2>
    <p><a href="/update">Go to OTA Update</a></p>
  </body>
</html>
)rawliteral";

void setup() {
  Serial.begin(9600);
  Serial.println("Servo test with OTA");

  // Init PWM driver
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);

  // Start Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // OTA update route
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", updatePage);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", homePage);
  });

  server.on("/update", HTTP_POST, 
    [](AsyncWebServerRequest *request){ request->send(200, "text/plain", (Update.hasError()) ? "Update Failed" : "Update Success. Rebooting..."); },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if (!index) {
        Serial.printf("Update Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      }
      if (!Update.hasError()) {
        if (Update.write(data, len) != len) Update.printError(Serial);
      }
      if (final) {
        if (Update.end(true)) {
          Serial.printf("Update complete\n");
          ESP.restart();
        } else {
          Update.printError(Serial);
        }
      }
    }
  );

  server.begin();
}

void loop() {
  // Test servo using setPWM()
  for (uint16_t pulselen = SERVOMIN; pulselen < SERVOMAX; pulselen++) {
    pwm.setPWM(servonum, 0, pulselen);
  }
  delay(500);
  for (uint16_t pulselen = SERVOMAX; pulselen > SERVOMIN; pulselen--) {
    pwm.setPWM(servonum, 0, pulselen);
  }
  delay(500);

  // Test servo using writeMicroseconds()
  for (uint16_t microsec = USMIN; microsec < USMAX; microsec++) {
    pwm.writeMicroseconds(servonum, microsec);
  }
  delay(500);
  for (uint16_t microsec = USMAX; microsec > USMIN; microsec--) {
    pwm.writeMicroseconds(servonum, microsec);
  }
  delay(500);

  servonum++;
  if (servonum > 7) servonum = 0;
}
