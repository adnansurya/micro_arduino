#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(D5, D6);

const int ledPin = D4;
bool manualMode = false;

// Buat access point
const char* ssid = "Wemos_AP";
const char* password = "12345678";

ESP8266WebServer server(80);  // Server pada port 80

void setup() {
  Serial.begin(9600);
  delay(1000);
  mySerial.begin(9600);

  pinMode(ledPin, OUTPUT);

  mySerial.println("mode:auto");

  // Mulai mode Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  // Tampilkan informasi AP
  Serial.println();
  Serial.println("Access Point dibuat!");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Setup routing untuk handle request
  server.on("/", handleRoot);         // Halaman utama
  server.on("/motor", handleMotor);   // Endpoint kontrol LED
  server.onNotFound(handleNotFound);  // Handle URL tidak ditemukan

  // Mulai server
  server.begin();
  Serial.println("HTTP server dimulai");
}

void loop() {
  if (manualMode) {
    digitalWrite(ledPin, LOW);
  } else {
    digitalWrite(ledPin, HIGH);
  }
  server.handleClient();  // Handle client requests
}

// Handler untuk halaman root
void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html lang='id'>";
  html += "<head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Remote Control Wemos</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }";
  html += ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { text-align: center; color: #333; }";
  html += ".status { text-align: center; padding: 10px; margin: 10px 0; border-radius: 5px; background: #e0e0e0; }";
  html += ".mode-buttons { display: flex; gap: 10px; margin-bottom: 20px; }";
  html += ".mode-btn { flex: 1; padding: 15px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; }";
  html += ".auto { background: #4CAF50; color: white; }";
  html += ".manual { background: #2196F3; color: white; }";
  html += ".control-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }";
  html += ".control-btn { padding: 20px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; background: #555; color: white; }";
  html += ".maju { grid-column: 2; grid-row: 1; background: #4CAF50; }";
  html += ".kiri { grid-column: 1; grid-row: 2; background: #FF9800; }";
  html += ".stop { grid-column: 2; grid-row: 2; background: #f44336; }";
  html += ".kanan { grid-column: 3; grid-row: 2; background: #FF9800; }";
  html += ".mundur { grid-column: 2; grid-row: 3; background: #2196F3; }";
  html += ".speed-control { margin-top: 20px; text-align: center; }";
  html += ".speed-btn { padding: 10px 15px; margin: 0 5px; border: none; border-radius: 5px; background: #666; color: white; cursor: pointer; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<h1>🤖 Remote Control</h1>";
  html += "<div class='status'>";
  html += "<strong>Mode: </strong>" + String(manualMode ? "MANUAL" : "AUTO");
  html += "</div>";
  
  // Mode Selection
  html += "<div class='mode-buttons'>";
  html += "<button class='mode-btn auto' onclick=\"sendCommand('mode=auto')\">AUTO</button>";
  html += "<button class='mode-btn manual' onclick=\"sendCommand('mode=manual')\">MANUAL</button>";
  html += "</div>";
  
  // Control Grid
  html += "<div class='control-grid'>";
  html += "<button class='control-btn maju' onclick=\"sendCommand('arah=maju')\">↑ MAJU</button>";
  html += "<button class='control-btn kiri' onclick=\"sendCommand('arah=kiri')\">← KIRI</button>";
  html += "<button class='control-btn stop' onclick=\"sendCommand('arah=stop')\">⏹ STOP</button>";
  html += "<button class='control-btn kanan' onclick=\"sendCommand('arah=kanan')\">→ KANAN</button>";
  html += "<button class='control-btn mundur' onclick=\"sendCommand('arah=mundur')\">↓ MUNDUR</button>";
  html += "</div>";
  
  // Speed Control
  html += "<div class='speed-control'>";
  html += "<h3>Kecepatan</h3>";
  html += "<button class='speed-btn' onclick=\"sendCommand('speed=80')\">Lambat</button>";
  html += "<button class='speed-btn' onclick=\"sendCommand('speed=120')\">Sedang</button>";
  html += "<button class='speed-btn' onclick=\"sendCommand('speed=160')\">Cepat</button>";
  html += "</div>";
  html += "</div>";
  
  html += "<script>";
  html += "function sendCommand(params) {";
  html += "  fetch('/motor?' + params)";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      console.log('Response:', data);";
  html += "      setTimeout(() => { window.location.href = '/'; }, 500);";
  html += "    })";
  html += "    .catch(error => {";
  html += "      console.error('Error:', error);";
  html += "      setTimeout(() => { window.location.href = '/'; }, 500);";
  html += "    });";
  html += "}";
  html += "</script>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

// Handler untuk endpoint /motor
void handleMotor() {
  String response = "";

  if (server.hasArg("mode")) {
    String mode = server.arg("mode");

    if (mode == "auto") {
      mySerial.println("mode:auto");
      manualMode = false;
      response = "MODE AUTO";
    } else if (mode == "manual") {
      mySerial.println("mode:manual");
      manualMode = true;
      response = "MODE MANUAL";
    } else {
      response = "Parameter mode tidak valid";
    }
  }

  if (server.hasArg("arah")) {
    String arah = server.arg("arah");

    if (arah == "stop") {
      mySerial.println("arah:stop");
      response = "ARAH : STOP";
    } else if (arah == "maju") {
      mySerial.println("arah:maju");
      response = "ARAH : MAJU";
    } else if (arah == "mundur") {
      mySerial.println("arah:mundur");
      response = "ARAH : MUNDUR";
    } else if (arah == "kiri") {
      mySerial.println("arah:kiri");
      response = "ARAH : KIRI";
    } else if (arah == "kanan") {
      mySerial.println("arah:kanan");
      response = "ARAH : KANAN";
    } else {
      response = "Parameter arah tidak valid";
    }
  }

  if (server.hasArg("speed")) {
    String speed = server.arg("speed");
    mySerial.println("speed:" + speed);
    response = "Speed : " + speed;
  }
  
  Serial.println(response);
  
  // Kirim response dan beri tahu client untuk redirect
  server.send(200, "text/plain", response + "\n\nRedirecting...");
}

// Handler untuk URL yang tidak ditemukan
void handleNotFound() {
  String message = "URL tidak ditemukan\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}