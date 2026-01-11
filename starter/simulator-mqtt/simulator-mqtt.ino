#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClientSecure.h>

// Konfigurasi WiFi
const char* ssid = "MIKRO";
const char* password = "1DEAlist";

// Konfigurasi MQTT Broker
const char* mqtt_server = "m5e61966.ala.asia-southeast1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_topic = "t/sensor/kebakaran";
const char* mqtt_username = "firealert";
const char* mqtt_password = "firealert";

// Alamat I2C untuk LCD
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// Inisialisasi LCD
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Variabel WiFi dan MQTT
WiFiClientSecure espClient;
PubSubClient client(espClient);

// Variabel nilai sensor simulasi
float temperature = 25.0;
float gas_ppm = 0.0;
int flame = 0;

// Variabel untuk nama dan label (dapat diubah)
String deviceName = "Hotel Delima Sari";
String deviceLabel = "hoteldelimasari";

// Variabel untuk menyimpan nilai sebelumnya
float prev_temperature = -100;
float prev_gas_ppm = -1;
int prev_flame = -1;

// Threshold perubahan
const float TEMP_THRESHOLD = 0.5;
const float GAS_THRESHOLD = 5.0;

// Timer untuk update LCD
unsigned long lastLcdUpdate = 0;
const long lcdInterval = 1000;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void updateLCD() {
  lcd.clear();
  
  // Baris pertama: Suhu dan Api
  lcd.setCursor(0, 0);
  lcd.print("S:");
  lcd.print(temperature, 1);
  lcd.print("C");
  
  lcd.setCursor(9, 0);
  lcd.print("A:");
  lcd.print(flame == 1 ? "Y" : "N");
  
  // Baris kedua: Gas LPG
  lcd.setCursor(0, 1);
  lcd.print("LPG:");
  lcd.print(gas_ppm, 1);
  lcd.print("ppm");
}

bool hasDataChanged() {
  bool changed = false;
  
  if (abs(temperature - prev_temperature) >= TEMP_THRESHOLD) {
    changed = true;
    prev_temperature = temperature;
  }
  
  if (abs(gas_ppm - prev_gas_ppm) >= GAS_THRESHOLD) {
    changed = true;
    prev_gas_ppm = gas_ppm;
  }
  
  if (flame != prev_flame) {
    changed = true;
    prev_flame = flame;
  }
  
  return changed;
}

String createSlug(String input) {
  String slug = "";
  input.toLowerCase();
  
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    // Hanya tambahkan huruf dan angka, abaikan spasi dan simbol
    if (isAlphaNumeric(c)) {
      slug += c;
    }
  }
  
  return slug;
}

void sendMQTTData() {
  // Buat JSON document
  StaticJsonDocument<256> doc;
  
  // Tambahkan data ke JSON
  doc["name"] = deviceName;
  doc["label"] = deviceLabel;
  doc["suhu"] = round(temperature * 100) / 100.0;
  doc["lpg"] = round(gas_ppm * 100) / 100.0;
  doc["api"] = flame;
  
  // Serialize JSON ke string
  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);
  
  // Publikasikan ke MQTT
  client.publish(mqtt_topic, jsonBuffer);
  
  // Print ke Serial Monitor
  Serial.println("Data sent (changed):");
  Serial.println(jsonBuffer);
  Serial.println();
}

void parseSerialCommand(String command) {
  command.trim();
  
  if (command.startsWith("suhu")) {
    String valueStr = command.substring(4);
    float newTemp = valueStr.toFloat();
    
    if (newTemp >= -50 && newTemp <= 100) {
      temperature = newTemp;
      Serial.print("Suhu diubah menjadi: ");
      Serial.println(temperature);
    } else {
      Serial.println("Error: Suhu harus antara -50 dan 100°C");
    }
  } 
  else if (command.startsWith("lpg")) {
    String valueStr = command.substring(3);
    float newGas = valueStr.toFloat();
    
    if (newGas >= 0 && newGas <= 10000) {
      gas_ppm = newGas;
      Serial.print("LPG diubah menjadi: ");
      Serial.println(gas_ppm);
    } else {
      Serial.println("Error: LPG harus antara 0 dan 10000 ppm");
    }
  } 
  else if (command.startsWith("api")) {
    String valueStr = command.substring(3);
    int newFlame = valueStr.toInt();
    
    if (newFlame == 0 || newFlame == 1) {
      flame = newFlame;
      Serial.print("Api diubah menjadi: ");
      Serial.println(flame == 1 ? "ADA" : "TIDAK ADA");
    } else {
      Serial.println("Error: Api harus 0 (tidak ada) atau 1 (ada)");
    }
  }
  else if (command.startsWith("name")) {
    // Ekstrak nama setelah "name"
    String newName = command.substring(4);
    newName.trim();
    
    if (newName.length() > 0) {
      deviceName = newName;
      
      // Generate label otomatis dari nama (tanpa spasi, langsung sambung)
      deviceLabel = createSlug(deviceName);
      
      Serial.print("Nama diubah menjadi: ");
      Serial.println(deviceName);
      Serial.print("Label otomatis: ");
      Serial.println(deviceLabel);
      
      // Kirim data ke MQTT karena nama/label berubah
      sendMQTTData();
    } else {
      Serial.println("Error: Nama tidak boleh kosong");
    }
  }
  else if (command == "help") {
    Serial.println("\n===== PERINTAH SIMULASI SENSOR =====");
    Serial.println("suhu<angka>    : Atur suhu (contoh: suhu40.0)");
    Serial.println("lpg<angka>     : Atur LPG (contoh: lpg4.5)");
    Serial.println("api<0/1>       : Atur status api (0=tidak ada, 1=ada)");
    Serial.println("name<spasi><teks> : Atur nama & label (contoh: name Masjid Al Ikhlas)");
    Serial.println("status         : Tampilkan semua nilai saat ini");
    Serial.println("info           : Tampilkan info device");
    Serial.println("sendnow        : Kirim data sekarang ke MQTT");
    Serial.println("help           : Tampilkan bantuan ini");
    Serial.println("===================================");
  }
  else if (command == "status") {
    Serial.println("\n===== STATUS SENSOR =====");
    Serial.print("Suhu: "); Serial.print(temperature); Serial.println(" °C");
    Serial.print("LPG : "); Serial.print(gas_ppm); Serial.println(" ppm");
    Serial.print("Api : "); Serial.println(flame == 1 ? "ADA" : "TIDAK ADA");
    Serial.println("=========================");
  }
  else if (command == "info") {
    Serial.println("\n===== INFO DEVICE =====");
    Serial.print("Nama  : "); Serial.println(deviceName);
    Serial.print("Label : "); Serial.println(deviceLabel);
    Serial.print("MQTT Server: "); Serial.println(mqtt_server);
    Serial.print("MQTT Topic : "); Serial.println(mqtt_topic);
    Serial.print("WiFi Status: "); Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    Serial.print("MQTT Status: "); Serial.println(client.connected() ? "Connected" : "Disconnected");
    Serial.println("=======================");
  }
  else if (command == "sendnow") {
    sendMQTTData();
    Serial.println("Data dikirim secara manual");
  }
  else {
    Serial.println("Perintah tidak dikenal. Ketik 'help' untuk bantuan.");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Simulasi Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Siap...");
  
  setup_wifi();
  setDateTime();

  espClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);
  
  // Inisialisasi nilai sebelumnya
  prev_temperature = temperature;
  prev_gas_ppm = gas_ppm;
  prev_flame = flame;
  
  delay(2000);
  lcd.clear();
  
  Serial.println("\n===== SISTEM SIMULASI SENSOR KEBARAN =====");
  Serial.println("Device Name : " + deviceName);
  Serial.println("Device Label: " + deviceLabel);
  Serial.println("Ketik 'help' untuk melihat semua perintah");
  Serial.println("===========================================");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Baca input dari Serial Monitor
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    parseSerialCommand(command);
  }
  
  unsigned long now = millis();
  
  // Update LCD secara berkala
  if (now - lastLcdUpdate > lcdInterval) {
    lastLcdUpdate = now;
    updateLCD();
  }
  
  // Cek apakah ada data sensor yang berubah
  if (hasDataChanged()) {
    sendMQTTData();
  }
  
  delay(100);
}

void setDateTime() {
  Serial.print("Menunggu sinkronisasi waktu NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println("\nWaktu telah disinkronkan.");
}