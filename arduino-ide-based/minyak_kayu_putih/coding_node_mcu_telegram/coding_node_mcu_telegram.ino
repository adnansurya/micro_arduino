#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Ganti dengan SSID dan Password WiFi Anda
const char* ssid = "SINURA";
const char* password = "12345670";

// const char* ssid = "MIKRO";
// const char* password = "IDEAlist";

// Ganti dengan token bot Telegram Anda
#define BOT_TOKEN "7340698145:AAFQV7EHNfhBkkWQHRZNiebf0HVEOB-5uo0"
#define CHAT_ID "6215788960"


X509List cert(TELEGRAM_CERTIFICATE_ROOT);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

float lastPhValue = 0.0;
float lastTemperature = 0.0;
int lastMq3Value = 0;
String lastJenisCairanPH = "Tidak Dikenal";
String lastJenisCairanWarna = "Tidak Dikenal";

// Variabel untuk warna
unsigned int redFrequency = 0;
unsigned int greenFrequency = 0;
unsigned int blueFrequency = 0;

unsigned long lastTimeSent = 0;
const unsigned long delayBetweenSends = 5000;  // 8 detik

void setup() {
  Serial.begin(9600);

  // Koneksi ke WiFi
  WiFi.begin(ssid, password);
  // client.setInsecure(); // Gunakan ini jika Anda tidak menggunakan sertifikat root
  client.setTrustAnchors(&cert);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Menghubungkan ke WiFi...");
  }
  Serial.println("Terhubung ke WiFi");

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  // Konfigurasi client untuk sertifikat root server Telegram


  // Debug: Cek koneksi ke server Telegram
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Koneksi ke Telegram Gagal");
  } else {
    Serial.println("Koneksi ke Telegram Berhasil");
    bot.sendMessage(CHAT_ID, "Koneksi ke Telegram Berhasil", "");
  }
}

void loop() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    Serial.println("Data dari Arduino: " + data);  // Debugging

    // Parsing data dari Arduino
    if (data.startsWith("PH:")) {
      String pHValue = data.substring(3);
      lastPhValue = pHValue.toFloat();
      // Deteksi jenis cairan pH
      lastJenisCairanPH = deteksiCairanPH(lastPhValue);
      Serial.println("Nilai PH: " + String(lastPhValue));
    } else if (data.startsWith("Temp:")) {
      String tempValue = data.substring(5);
      lastTemperature = tempValue.toFloat();
    } else if (data.startsWith("MQ3:")) {
      String mq3Value = data.substring(4);
      lastMq3Value = mq3Value.toInt();
    } else if (data.startsWith("Merah:")) {
      String redValue = data.substring(6);
      redFrequency = redValue.toInt();                         // Simpan nilai merah
      Serial.println("Nilai Merah: " + String(redFrequency));  // Debugging
    } else if (data.startsWith("Hijau:")) {
      String greenValue = data.substring(6);
      greenFrequency = greenValue.toInt();                       // Simpan nilai hijau
      Serial.println("Nilai Hijau: " + String(greenFrequency));  // Debugging
    } else if (data.startsWith("Biru:")) {
      String blueValue = data.substring(5);
      blueFrequency = blueValue.toInt();                       // Simpan nilai biru
      Serial.println("Nilai Biru: " + String(blueFrequency));  // Debugging
      // Update jenis cairan warna berdasarkan warna terakhir yang dibaca
      lastJenisCairanWarna = deteksiCairanWarna();
      Serial.println("Jenis Cairan Warna: " + lastJenisCairanWarna);  // Debugging
    }
  }

  if (millis() - lastTimeSent >= delayBetweenSends) {


    // Periksa pesan dari bot Telegram
    Serial.print("last update : ");
    Serial.println(bot.last_message_received);
    int numNewMessages = bot.getUpdates(-1);
    Serial.print("NM : ");
    Serial.println(numNewMessages);

    while (numNewMessages) {
      for (int i = 0; i < numNewMessages; i++) {
        String chat_id = String(bot.messages[i].chat_id);
        String text = bot.messages[i].text;
        String msg_id = String(bot.messages[i].message_id);
        
        Serial.print("msg_id : ");
        Serial.println(msg_id);

        if (chat_id == CHAT_ID) {
          if (text == "/ph") {
            // Kirim nilai pH dan jenis cairan terakhir
            String message = "Nilai pH terakhir: " + String(lastPhValue, 2) + "\nJenis Cairan pH: " + lastJenisCairanPH;
            if (bot.sendMessage(CHAT_ID, message, "")) {
              Serial.println("Pesan Telegram Terkirim: " + message);
            } else {
              Serial.println("Gagal Mengirim Pesan Telegram : " +message);
            }
          } else if (text == "/temp") {
            // Kirim suhu terakhir
            String message = "Suhu terakhir: " + String(lastTemperature, 1) + " C";
            if (bot.sendMessage(CHAT_ID, message, "")) {
              Serial.println("Pesan Telegram Terkirim: " + message);
            } else {
              Serial.println("Gagal Mengirim Pesan Telegram : " +message);
            }
          } else if (text == "/mq3") {
            // Kirim nilai MQ3 terakhir
            String message = "Nilai MQ3 terakhir: " + String(lastMq3Value);
            if (bot.sendMessage(CHAT_ID, message, "")) {
              Serial.println("Pesan Telegram Terkirim: " + message);
            } else {
              Serial.println("Gagal Mengirim Pesan Telegram : " +message);
            }
          } else if (text == "/warna") {
            // Kirim hasil deteksi warna terakhir
            String message = "Jenis Cairan Berdasarkan Warna: " + lastJenisCairanWarna;
            if (bot.sendMessage(CHAT_ID, message, "")) {
              Serial.println("Pesan Telegram Terkirim: " + message);
            } else {
              Serial.println("Gagal Mengirim Pesan Telegram : " +message);
            }
          } else {
            if (bot.sendMessage(CHAT_ID, "Perintah tidak dikenali. Gunakan /ph untuk mendapatkan nilai pH, /temp untuk suhu, /mq3 untuk nilai MQ3, dan /warna untuk frekuensi warna.", "")) {
              Serial.println("Pesan Telegram Terkirim: Perintah tidak dikenali.");
            } else {
              Serial.println("Gagal Mengirim Pesan Telegram: Perintah tidak dikenali.");
            }
          }
        }
      }
       numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeSent = millis();  // Update waktu terakhir pengiriman
  }
}

String deteksiCairanPH(float Po) {
  if (Po > 7.50) {
    return "Minyak Telon";
  } else if (Po <= 6.50) {
    return "Minyak Penyuling";
  } else {
    return "Tidak Dikenal";
  }
}

String deteksiCairanWarna() {
  // Deteksi jenis cairan berdasarkan warna
  if (redFrequency >= 14440 && redFrequency <= 14800 && greenFrequency >= 10750 && greenFrequency <= 17000 && blueFrequency >= 13873 && blueFrequency <= 23800) {
    return "Minyak Telon";
  } else if (redFrequency >= 10150 && redFrequency <= 16058 && greenFrequency >= 11400 && greenFrequency <= 17345 && blueFrequency >= 14653 && blueFrequency <= 23789) {
    return "Minyak Penyuling";
  } else {
    return "Tidak Dikenal";
  }
}
