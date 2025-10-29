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
  String html = "<html><head><title>Wemos D1 Mini Server</title></head>";
  html += "<body>";
  html += "<h1>Wemos D1 Mini Web Server</h1>";
  html += "<p>Access Point berhasil dibuat!</p>";
  html += "<p>Gunakan URL berikut untuk testing:</p>";
  html += "<ul>";
  html += "<li><a href='/motor?mode=auto'>MODE AUTO</a></li>";
  html += "<li><a href='/motor?mode=manual'>MODE MANUAL</a></li>";
  html += "<li><a href='/motor?arah=stop'>STOP</a></li>";
  html += "<li><a href='/motor?arah=maju'>MAJU</a></li>";
  html += "<li><a href='/motor?arah=mundur'>MUNDUR</a></li>";
  html += "<li><a href='/motor?arah=kiri'>KIRI</a></li>";
  html += "<li><a href='/motor?arah=kanan'>KANAN</a></li>";
  html += "</ul>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Handler untuk endpoint /data
void handleData() {
  String message = "Data received:\n";

  // Ambil semua parameter yang dikirim
  for (int i = 0; i < server.args(); i++) {
    message += "Parameter " + String(i) + ": ";
    message += server.argName(i) + " = " + server.arg(i) + "\n";
  }

  // Tampilkan di serial monitor
  Serial.println("=== Data Received ===");
  for (int i = 0; i < server.args(); i++) {
    Serial.print(server.argName(i));
    Serial.print(": ");
    Serial.println(server.arg(i));
  }
  Serial.println("=====================");

  // Kirim response ke client
  server.send(200, "text/plain", message);
}

// Handler untuk endpoint /led (kontrol LED onboard)
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

  server.send(200, "text/plain", response);
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