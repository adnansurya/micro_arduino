#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

// PCA9685 servo driver
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Mapping channel PCA9685
#define SERVO1_CH 0
#define SERVO2_CH 1
#define SERVO3_CH 2
#define SERVO4_CH 3

// Server
AsyncWebServer server(80);

// Fungsi konversi sudut ke pulsa PCA9685
int angleToPulse(int angle) {
  return map(angle, 0, 180, 102, 512); // 500us–2400us pada skala 0–4095
}

// HTML halaman kontrol
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Kontrol Lengan Robot</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; padding: 20px; }
    input[type=range] { width: 80%%; }
    button { padding: 10px 20px; font-size: 16px; }
  </style>
</head>
<body>
  <h2>Kendali Lengan Robot OTA</h2>
  <div>
    <label>Bawah - (<span id="s1val">90</span>&#176;) - Atas</label><br>
    <input type="range" min="0" max="180" value="90" id="s1" oninput="updateServo(this.value,1)">
  </div><br>
  <div>
    <label>Kiri - (<span id="s2val">90</span>&#176;) - Kanan</label><br>
    <input type="range" min="0" max="180" value="90" id="s2" oninput="updateServo(this.value,2)">
  </div><br>
  <div>
    <label>Mundur - (<span id="s3val">110</span>&#176;) - Maju</label><br>
    <input type="range" min="50" max="170" value="110" id="s3" oninput="updateServo(this.value,3)">
  </div><br>
  <div>
    <label>Lepas - (<span id="s4val">50</span>&#176;) - Jepit</label><br>
    <input type="range" min="30" max="70" value="50" id="s4" oninput="updateServo(this.value,4)">
  </div><br>
  <button onclick="jepit()">JEPIT OTOMATIS</button>

  <script>
    function updateServo(val, id) {
      document.getElementById("s" + id + "val").innerText = val;
      fetch(`/servo?id=${id}&val=${val}`);
    }
    function jepit() {
      fetch("/jepit");
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Wire.begin(); // SDA: GPIO21, SCL: GPIO22
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);

  // Posisi awal servo
  pwm.setPWM(SERVO1_CH, 0, angleToPulse(90));
  pwm.setPWM(SERVO2_CH, 0, angleToPulse(90));
  pwm.setPWM(SERVO3_CH, 0, angleToPulse(110));
  pwm.setPWM(SERVO4_CH, 0, angleToPulse(50));

  // Jalankan sebagai Access Point
  const char* ssid = "LenganRobotESP32";
  const char* password = "12345678";
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Routing utama
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Kendali servo via GET
  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id") && request->hasParam("val")) {
      int id = request->getParam("id")->value().toInt();
      int val = constrain(request->getParam("val")->value().toInt(), 0, 180);
      switch (id) {
        case 1: pwm.setPWM(SERVO1_CH, 0, angleToPulse(val)); break;
        case 2: pwm.setPWM(SERVO2_CH, 0, angleToPulse(180 - val)); break;
        case 3: pwm.setPWM(SERVO3_CH, 0, angleToPulse(val)); break;
        case 4: pwm.setPWM(SERVO4_CH, 0, angleToPulse(val)); break;
      }
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });

  // Tombol Jepit Otomatis
  server.on("/jepit", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Jepit otomatis dimulai");
    jepitOtomatis();
  });

  // OTA Update
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html",
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update'><input type='submit' value='Upload'>"
      "</form>");
  });

  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      bool hasError = Update.hasError();
      request->send(200, "text/plain", hasError ? "Update Gagal" : "Update Berhasil. ESP restart...");
      if (!hasError) {
        delay(1000);
        ESP.restart();
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("Upload Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      }
      if (!Update.hasError()) {
        if (Update.write(data, len) != len) Update.printError(Serial);
      }
      if (final) {
        if (Update.end(true)) Serial.printf("Update Success: %u bytes\n", index + len);
        else Update.printError(Serial);
      }
    });

  server.begin();
}

void loop() {
  // Kosong
}

void jepitOtomatis() {
  const int lepas = 50;
  const int jepit = 70;
  const int jeda = 500;
  const int ulang = 5;
  for (int i = 0; i < ulang; i++) {
    pwm.setPWM(SERVO4_CH, 0, angleToPulse(jepit));
    delay(jeda);
    pwm.setPWM(SERVO4_CH, 0, angleToPulse(lepas));
    delay(jeda);
  }
}
