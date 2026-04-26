#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

// --- VARIABEL WARNA (TEMA PUTIH) ---
uint16_t COLOR_BG      = TFT_WHITE;     
uint16_t COLOR_TEXT    = TFT_BLACK;     
uint16_t COLOR_ACCENT  = TFT_BLUE;      
uint16_t COLOR_NAV_BG  = 0xE71C;        
uint16_t COLOR_NAV_TXT = 0x0010;        
uint16_t COLOR_TITLE   = 0x03E0;        
uint16_t COLOR_ARTIST  = 0x4208;        
uint16_t COLOR_STATUS  = TFT_MAGENTA;   
uint16_t COLOR_VOL     = 0x001F;        
uint16_t COLOR_BAR_BG  = 0xD6BA;        
uint16_t COLOR_BAR_FILL = 0x07FF;       
uint16_t COLOR_DATE    = 0x7BEF; 

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristicTX;
bool deviceConnected = false;
String artist = "-", title = "-", currentTime = "00:00";
String navInstr = "", navDist = "", navEta = "";
String musicState = "pause";
int volumeLevel = 0;
long positionSec = 0, durationSec = 0;
time_t lastUnixTime = 0; // Simpan timestamp secara global
String inputBuffer = "";
bool refreshDisplay = true;
bool isNavigating = false;

String formatTime(long seconds) {
  if (seconds < 0) seconds = 0;
  int m = seconds / 60;
  int s = seconds % 60;
  char buf[10];
  sprintf(buf, "%02d:%02d", m, s);
  return String(buf);
}

void updateDisplay() {
  if (!deviceConnected) {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_ARTIST);
    tft.setTextSize(2);
    tft.drawCentreString("DISCONNECTED", tft.width() / 2, 120, 1);
    return;
  }

  tft.setRotation(0);
  tft.fillScreen(COLOR_BG);
  
  // 1. Header Jam
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(4);
  tft.drawCentreString(currentTime, tft.width() / 2, 20, 1);
  tft.drawFastHLine(20, 65, tft.width() - 40, COLOR_ACCENT);

  // 2. Panel Navigasi
  if (isNavigating) {
    tft.fillRoundRect(5, 75, tft.width()-10, 90, 8, COLOR_NAV_BG);
    tft.setTextColor(COLOR_NAV_TXT);
    tft.setTextSize(1);
    tft.drawString("NAVIGASI", 15, 82);
    tft.setTextSize(2);
    tft.drawString(navDist, 15, 95);
    String shortInstr = navInstr.length() > 18 ? navInstr.substring(0, 16) + ".." : navInstr;
    tft.drawCentreString(shortInstr, tft.width() / 2, 125, 1);
    if (navEta != "") {
      tft.setTextSize(1);
      tft.drawRightString("ETA: " + navEta, tft.width() - 15, 82, 1);
    }
  } else {
    tft.drawRoundRect(5, 75, tft.width()-10, 40, 8, COLOR_NAV_BG);
    tft.setTextColor(COLOR_ARTIST);
    tft.setTextSize(1);
    tft.drawCentreString("Navigasi tidak tersedia", tft.width() / 2, 88, 1);
  }

  // 3. Panel Musik
  int musicY = isNavigating ? 180 : 130;
  tft.setTextColor(COLOR_TITLE);
  tft.setTextSize(2);
  String shortTitle = title.length() > 16 ? title.substring(0, 14) + ".." : title;
  tft.drawCentreString(shortTitle, tft.width() / 2, musicY, 1);
  
  tft.setTextColor(COLOR_ARTIST);
  tft.setTextSize(2);
  String shortArtist = artist.length() > 18 ? artist.substring(0, 16) + ".." : artist;
  tft.drawCentreString(shortArtist, tft.width() / 2, musicY + 30, 1);

  tft.setTextColor(COLOR_STATUS);
  tft.setTextSize(1);
  String statusTxt = (musicState == "play") ? "NOW PLAYING" : "PAUSED (" + formatTime(positionSec) + "/" + formatTime(durationSec) + ")";
  tft.drawCentreString(statusTxt, tft.width() / 2, musicY + 60, 1);

  // 4. Panel Volume
  int volY = 265;
  tft.setTextColor(COLOR_VOL);
  tft.setTextSize(1);
  tft.drawCentreString("VOLUME " + String(volumeLevel) + "%", tft.width() / 2, volY, 1);
  tft.fillRoundRect((tft.width() - 160) / 2, volY + 15, 160, 8, 4, COLOR_BAR_BG);
  int progress = map(volumeLevel, 0, 100, 0, 160);
  if (progress > 0) tft.fillRoundRect((tft.width() - 160) / 2, volY + 15, progress, 8, 4, COLOR_BAR_FILL);

  // 5. TANGGAL (Paling Bawah)
  if (lastUnixTime > 0) {
    struct tm *tmp = localtime(&lastUnixTime);
    const char* hari[] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
    const char* bulan[] = {"Jan", "Feb", "Mar", "Apr", "Mei", "Jun", "Jul", "Agu", "Sep", "Okt", "Nov", "Des"};
    char dateBuf[40];
    sprintf(dateBuf, "%s, %02d %s %d", hari[tmp->tm_wday], tmp->tm_mday, bulan[tmp->tm_mon], tmp->tm_year + 1900);
    tft.setTextColor(COLOR_DATE);
    tft.setTextSize(1);
    tft.drawCentreString(dateBuf, tft.width() / 2, 305, 1);
  }
}

void processBuffer(String data) {
  Serial.println(data);
  if (data.indexOf("setTime(") != -1) {
    int startIdx = data.indexOf("(") + 1;
    int endIdx = data.indexOf(")");
    lastUnixTime = data.substring(startIdx, endIdx).toInt(); // Update timestamp global
    struct tm *tmp = localtime(&lastUnixTime);
    char buf[10];
    sprintf(buf, "%02d:%02d", tmp->tm_hour, tmp->tm_min);
    currentTime = String(buf);
    refreshDisplay = true;
    return;
  }
  int start = data.indexOf("GB({");
  int end = data.lastIndexOf("})");
  if (start != -1 && end != -1) {
    String jsonStr = data.substring(start + 3, end + 2);
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, jsonStr) == DeserializationError::Ok) {
      String type = doc["t"] | "";
      if (type == "musicinfo") {
        title = doc["track"] | "No Title";
        artist = doc["artist"] | "No Artist";
        durationSec = doc["dur"] | 0;
        refreshDisplay = true;
      } 
      else if (type == "musicstate") {
        musicState = doc["state"].as<String>();
        positionSec = doc["position"] | 0;
        refreshDisplay = true;
      }
      else if (type == "nav") {
        navInstr = doc["instr"] | "";
        String rawDist = doc["distance"] | "";
        if (rawDist.length() >= 2) rawDist.setCharAt(rawDist.length() - 2, ' ');
        navDist = rawDist;
        navEta = doc["eta"] | "";
        isNavigating = (navInstr != "" && navInstr != " ");
        refreshDisplay = true;
      }
      else if (type == "audio") {
        volumeLevel = doc["v"];
        refreshDisplay = true;
      }
    }
  }
}

// --- Fungsi untuk mengirim balik command ke Gadgetbridge ---
void sendToGB(String cmd) {
  if (deviceConnected) {
    pCharacteristicTX->setValue(cmd.c_str());
    pCharacteristicTX->notify();
    Serial.println("Sent to GB: " + cmd);
  }
}

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    for (char c : value) {
      if (c == '\n') { processBuffer(inputBuffer); inputBuffer = ""; }
      else { inputBuffer += c; }
    }
  }
};

// --- Update MyServerCallbacks ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      refreshDisplay = true;
      
      // TRIGGER: Beritahu Gadgetbridge bahwa kita siap menerima data
      // Kirim delay sedikit agar koneksi stabil dulu
      delay(500); 
      
      // 1. Kirim info versi (pura-pura jadi Bangle.js yang valid)
      sendToGB("GB({\"t\":\"info\",\"v\":\"2v25\",\"m\":\"ESP32-Dashboard\"})\n");
      
      // 2. Minta status GPS (agar navigasi terpancing)
      sendToGB("GB({\"t\":\"gps\",\"status\":\"ok\"})\n");
      
      // 3. Minta info musik terbaru
      sendToGB("GB({\"t\":\"music\",\"n\":\"info\"})\n");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      refreshDisplay = true;
      pServer->getAdvertising()->start();
    }
};

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  BLEDevice::init("Bangle.js");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristicTX = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristicTX->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristicRX = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pCharacteristicRX->setCallbacks(new MyCallbacks());
  pService->start();
  pServer->getAdvertising()->start();
  updateDisplay();
}

void loop() {
  if (refreshDisplay) { updateDisplay(); refreshDisplay = false; }
  delay(100);
}