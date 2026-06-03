#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
BLECharacteristic* pRxCharacteristic = NULL;
bool deviceConnected = false;

// Tambahkan variabel buffer global untuk menampung data yang terpotong
String bleBuffer = ""; 

// UUID Standar Nordic UART
#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID                "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define TX_UUID                "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// Callback untuk mendeteksi koneksi
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      bleBuffer = ""; // Reset buffer saat koneksi baru
      Serial.println("Gadgetbridge Terhubung!");
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Gadgetbridge Terputus...");
      pServer->getAdvertising()->start();
    }
};

// Callback perbaikan untuk menyatukan data yang terpotong
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      // 1. Ambil potongan data yang baru masuk
      String chunk = pCharacteristic->getValue().c_str();
      
      // 2. Masukkan ke dalam buffer global
      bleBuffer += chunk;

      // 3. Periksa apakah di dalam buffer sudah ada karakter penutup baris ('\n')
      if (bleBuffer.indexOf('\n') != -1) {
        
        // Bersihkan karakter aneh di awal (seperti karakter kontrol 0x10 atau \n)
        bleBuffer.trim(); 
        
        // Pastikan datanya tidak kosong setelah dibersihkan
        if (bleBuffer.length() > 0) {
          Serial.println("====== DATA UTUH MASUK ======");
          Serial.println(bleBuffer); 
          Serial.println("=============================");
          
          // -----------------------------------------------------------
          // DISINI TEMPAT UNTUK PARSING (WAKTU, MUSIK, NAVIGASI) NANTI
          // -----------------------------------------------------------
        }
        
        // 4. Kosongkan buffer setelah data utuh berhasil diproses
        bleBuffer = ""; 
      }
    }
};

// ... Bagian void setup() dan void loop() tetap sama seperti kode kamu sebelumnya ...

void setup() {
  Serial.begin(115200);

  // Inisialisasi BLE Device
  BLEDevice::init("Bangle.js CYD"); // Nama ini yang akan muncul di Gadgetbridge

  // Create Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create TX Characteristic (Untuk kirim perintah balik ke HP, misal media control)
  pTxCharacteristic = pService->createCharacteristic(
                        TX_UUID,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Create RX Characteristic (Untuk menerima data dari HP)
  pRxCharacteristic = pService->createCharacteristic(
                        RX_UUID,
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_WRITE_NR
                      );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start Service
  pService->start();

  // Start Advertising agar terdeteksi di HP
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("Mulai mencari koneksi Gadgetbridge...");
}

void loop() {
    // Jalankan logika berkala di sini jika diperlukan
    delay(10);
}