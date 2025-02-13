#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#define FLASH_LED_PIN 2
#define RELAY_PIN 5
#define LDR_PIN 35


// Konfigurasi WiFi
#define WIFI_SSID "MIKRO"         // Ganti dengan SSID WiFi Anda
#define WIFI_PASSWORD "1DEAlist"  // Ganti dengan password WiFi Anda

#define DATABASE_URL "https://pintucerdas-4b1ae-default-rtdb.asia-southeast1.firebasedatabase.app/"

String dirPath = "/logs";
time_t tstamp;

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase Database;
AsyncResult result;
NoAuth noAuth;

const int lightLimit = 500;
bool terbuka = false;
bool lastTerbuka = false;

void setup() {
  // put your setup code here, to run once:
  // Init Serial Monitor
  Serial.begin(115200);

  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  digitalWrite(FLASH_LED_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // ledBlink(2, 100);
  Serial.println();

  Serial.print("Retrieving time: ");
  configTime(0, 0, "id.pool.ntp.org", "pool.ntp.org");  // get UTC time via NTP
  tstamp = getTimestamp();

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  ssl.setInsecure();
#if defined(ESP8266)
  ssl.setBufferSizes(1024, 1024);
#endif

  // Initialize the authentication handler.
  initializeApp(client, app, getAuth(noAuth));

  // Binding the authentication handler with your Database class object.
  app.getApp<RealtimeDatabase>(Database);

  // Set your database URL
  Database.url(DATABASE_URL);

  // In sync functions, we have to set the operating result for the client that works with the function.
  client.setAsyncResult(result);
}

time_t getTimestamp() {
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  return now;
}


void loop() {
  // put your main code here, to run repeatedly:
  int adcLdr = analogRead(LDR_PIN);
  Serial.print("LDR: ")
  Serial.println(adcLdr);

  if(adcLdr < lightLimit){
    terbuka = true;
  }else{
    terbuka = false;
  }

  if(terbuka != lastTerbuka){
    Serial.print("Terbuka: ");
    Serial.println(terbuka);
    delay(1000);
  }

  lastTerbuka = terbuka;

  delay(10);

}

void setStringFb(String str, String fbDir) {
  Serial.print("Set string... ");
  bool status = Database.set<String>(client, fbDir, str);
  if (status)
    Serial.println("ok");
  else
    printError(client.lastError().code(), client.lastError().message());
}

// Fungsi untuk mengonversi timestamp ke format tanggal dan waktu
String convertTimestampToDateTime(unsigned long timestamp, long offsetHour) {
    struct tm timeinfo;
    time_t rawtime = (time_t) (timestamp + (offsetHour * 3600));

    // Konversi timestamp ke struct tm
    localtime_r(&rawtime, &timeinfo);

    // Format hasil menjadi string (YYYY-MM-DD HH:MM:SS)
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    
    return String(buffer);
}

void printError(int code, const String& msg) {
  Firebase.printf("Error, msg: %s, code: %d\n", msg.c_str(), code);
}
