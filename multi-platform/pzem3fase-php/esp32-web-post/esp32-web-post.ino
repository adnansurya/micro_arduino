#include <WiFi.h>
#include <HTTPClient.h>

// Informasi WiFi
const char* ssid = "Your_SSID";
const char* password = "Your_PASSWORD";

// URL API
const char* serverName = "http://your_server_ip/api/realtime-data.php";

// Dummy data pengukuran arus
float phaseA = 0.0;
float phaseB = 0.0;
float phaseC = 0.0;

void setup() {
    Serial.begin(115200);

    // Menghubungkan ke WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        // Generate data dummy
        phaseA = random(10, 50) / 10.0; // Simulasi arus Phase A
        phaseB = random(10, 50) / 10.0; // Simulasi arus Phase B
        phaseC = random(10, 50) / 10.0; // Simulasi arus Phase C

        // Menyiapkan HTTP POST
        http.begin(serverName);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");

        String httpRequestData = "phase_a=" + String(phaseA) +
                                 "&phase_b=" + String(phaseB) +
                                 "&phase_c=" + String(phaseC);

        int httpResponseCode = http.POST(httpRequestData);

        // Menampilkan respons
        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("HTTP Response code: " + String(httpResponseCode));
            Serial.println("Response: " + response);
        } else {
            Serial.println("Error in sending POST: " + String(httpResponseCode));
        }

        http.end();
    } else {
        Serial.println("WiFi Disconnected");
    }

    delay(2000); // Kirim data setiap 2 detik
}
