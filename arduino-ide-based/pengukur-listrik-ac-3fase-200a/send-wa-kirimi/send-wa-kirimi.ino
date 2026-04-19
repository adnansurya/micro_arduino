#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* apiUrl = "https://api.kirimi.id/v1/send-message";

// Variabel untuk menampung input
String user_code, secret, device_id, phone, message;

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");

  // Mulai proses input
  inputData();
}

void loop() {
  // Jika ingin mengulang pengiriman, bisa panggil inputData() lagi di sini dengan kondisi tertentu
}

void inputData() {
  Serial.println("\n--- Konfigurasi Pesan WhatsApp ---");
  
  user_code = readSerial("Masukkan User Code: ");
  secret    = readSerial("Masukkan Secret: ");
  device_id = readSerial("Masukkan Device ID: ");
  phone     = readSerial("Masukkan Nomor HP (contoh: 628xxx): ");
  message   = readSerial("Masukkan Pesan: ");

  Serial.println("\nSemua data diterima. Mengirim pesan...");
  sendMessage();
}

// Fungsi pembantu untuk membaca input Serial
String readSerial(String prompt) {
  Serial.print(prompt);
  while (!Serial.available()) {
    delay(10); // Tunggu sampai ada input
  }
  String input = Serial.readStringUntil('\n');
  input.trim(); // Menghapus spasi atau karakter newline (\r \n)
  Serial.println(input); // Tampilkan kembali apa yang diketik
  return input;
}

void sendMessage() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<512> doc;
    doc["user_code"] = user_code;
    doc["secret"] = secret;
    doc["device_id"] = device_id;
    doc["phone"] = phone;
    doc["message"] = message;

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.println("Response: " + http.getString());
    } else {
      Serial.print("Error: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
  
  Serial.println("\n--- Selesai ---");
}