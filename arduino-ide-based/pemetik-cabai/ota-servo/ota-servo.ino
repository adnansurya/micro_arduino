#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <Update.h>

// Objek servo
Servo servo1, servo2, servo3, servo4;

// Pin GPIO servo
const int SERVO1_PIN = 14;
const int SERVO2_PIN = 27;
const int SERVO3_PIN = 26;
const int SERVO4_PIN = 25;

// Server
AsyncWebServer server(80);

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
  </style>
</head>
<body>
  <h2>Kendali Lengan Robot OTA</h2>
  <div>
    <label>Atas - (<span id="s1val">90</span>&#176;) - Bawah</label><br>
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
    <script>
      function updateServo(val, id) {
        document.getElementById("s" + id + "val").innerText = val;
        fetch(`/servo?id=${id}&val=${val}`);
      }
    </script>
  </body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  // Attach servo ke GPIO
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);
  servo4.setPeriodHertz(50);

  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  servo3.attach(SERVO3_PIN, 500, 2400);
  servo4.attach(SERVO4_PIN, 500, 2400);

  // Posisi awal
  servo1.write(90);
  servo2.write(90);
  servo3.write(110);
  servo4.write(50);

  // Jalankan sebagai Access Point
  const char *ssid = "LenganRobotESP32";
  const char *password = "12345678";
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Routing utama
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Kendali servo via GET parameter
  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id") && request->hasParam("val")) {
      int id = request->getParam("id")->value().toInt();
      int val = request->getParam("val")->value().toInt();
      val = constrain(val, 0, 180);

      switch (id) {
        case 1: servo1.write(val); break;
        case 2: servo2.write(180 - val); break;
        case 3: servo3.write(val); break;
        case 4: servo4.write(val); break;
      }

      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html",
                  "<form method='POST' action='/update' enctype='multipart/form-data'>"
                  "<input type='file' name='update'><input type='submit' value='Upload'>"
                  "</form>");
  });

  server.on("/jepit", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Jepit otomatis dimulai");
    jepitOtomatis();
  });

  // Proses OTA update
  server.on(
    "/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      bool updateHasError = Update.hasError();
      request->send(200, "text/plain", updateHasError ? "Update Gagal" : "Update Berhasil. ESP akan restart.");
      if (!updateHasError) {
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
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
        }
      }
      if (final) {
        if (Update.end(true)) {
          Serial.printf("Update Success: %u bytes\n", index + len);
        } else {
          Update.printError(Serial);
        }
      }
    });

  server.begin();
}


void loop() {
  // Tidak digunakan
}

void jepitOtomatis() {
  const int lepas = 50;
  const int jepit = 70;
  const int jeda = 200; // delay per gerakan (ms)
  const int ulang = 10;  // berapa kali gerakan jepit-lepas

  for (int i = 0; i < ulang; i++) {
    servo4.write(jepit);
    delay(jeda);
    servo4.write(lepas);
    delay(jeda);
  }
}
