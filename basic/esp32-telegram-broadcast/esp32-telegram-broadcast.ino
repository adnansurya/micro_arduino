#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <vector> 

// Wifi network station credentials
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "1DEAlist"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "8781341537:AAFa3l_WPome0qOHzt9X73emdHGAT3oIBsM"

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

// --- KONFIGURASI RELAY (ESP32 38-PIN) ---
const int RELAY_PIN = 26;       // Relay Channel 1 (Kontrol Beban)
const int READY_INDICATOR_PIN = 27; // Relay Channel 2 (Indikator Sistem Siap) -> Menggunakan GPIO 27 yang aman

// Ubah ke false jika relay Anda jenis Active High (menyala saat HIGH)
bool isRelayActiveLow = true;

// Struktur data untuk menyimpan Chat ID unik di dalam RAM
std::vector<String> savedChatIds;

// Fungsi untuk mengecek dan menyimpan Chat ID baru jika belum terdaftar
void registerChatId(String chat_id) {
  bool alreadyExists = false;
  for (String id : savedChatIds) {
    if (id == chat_id) {
      alreadyExists = true;
      break;
    }
  }
  
  if (!alreadyExists) {
    savedChatIds.push_back(chat_id);
    Serial.println("Chat ID Baru Terdaftar: " + chat_id);
  }
}

// Fungsi broadcast yang dimodifikasi agar TIDAK mengirim ulang ke si pengirim aksi (sender_id)
void broadcastMessage(String message, String sender_id) {
  Serial.println("Mengirim Broadcast: " + message);
  for (String id : savedChatIds) {
    if (id != sender_id) { // Lewati jika id adalah orang yang mengeksekusi perintah
      bot.sendMessage(id, message, "");
    }
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    // Daftarkan Chat ID secara otomatis setiap kali ada pesan masuk
    registerChatId(chat_id);

    // Menampilkan menu awal saat user mengetik /start
    if (text == "/start") {
      String welcome = "Halo " + from_name + ".\n";
      welcome += "Gunakan perintah berikut untuk mengontrol relay:\n\n";
      welcome += "/relay_on : Menyalakan Relay\n";
      welcome += "/relay_off : Mematikan Relay\n";
      welcome += "/status : Cek Status Relay Saat Ini\n";
      bot.sendMessage(chat_id, welcome, "");
    }

    // Perintah untuk menyalakan relay
    else if (text == "/relay_on") {
      digitalWrite(RELAY_PIN, isRelayActiveLow ? LOW : HIGH);
      
      // Kirim konfirmasi ke pelaku langsung
      bot.sendMessage(chat_id, "Relay berhasil dinyalakan (ON).", "");
      
      // Broadcast notifikasi aksi ke semua ID lain yang terdaftar
      String broadcastText = "🔔 NOTIFIKASI: [" + from_name + "] telah MENYALAKAN Relay.";
      broadcastMessage(broadcastText, chat_id);
    }

    // Perintah untuk mematikan relay
    else if (text == "/relay_off") {
      digitalWrite(RELAY_PIN, isRelayActiveLow ? HIGH : LOW);
      
      // Kirim konfirmasi ke pelaku langsung
      bot.sendMessage(chat_id, "Relay berhasil dimatikan (OFF).", "");
      
      // Broadcast notifikasi aksi ke semua ID lain yang terdaftar
      String broadcastText = "🔔 NOTIFIKASI: [" + from_name + "] telah MEMATIKAN Relay.";
      broadcastMessage(broadcastText, chat_id);
    } 

    // Perintah untuk mengecek status relay saat ini
    else if (text == "/status") {
      int currentState = digitalRead(RELAY_PIN);
      bool isOn = isRelayActiveLow ? (currentState == LOW) : (currentState == HIGH);

      if (isOn) {
        bot.sendMessage(chat_id, "Status saat ini: Relay ON", "");
      } else {
        bot.sendMessage(chat_id, "Status saat ini: Relay OFF", "");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    delay(10);
  }
  Serial.println();

  // Inisialisasi pin Relay Kontrol sebagai OUTPUT
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, isRelayActiveLow ? HIGH : LOW); // Kondisi Awal: OFF

  // Inisialisasi pin Relay Indikator Ready sebagai OUTPUT
  pinMode(READY_INDICATOR_PIN, OUTPUT);
  digitalWrite(READY_INDICATOR_PIN, isRelayActiveLow ? HIGH : LOW); // Kondisi Awal: OFF (Belum siap)

  // Hubungkan ke WiFi
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setInsecure();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  // Pengambilan waktu NTP
  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  // --- SISTEM SIAP ---
  // Menyalakan Relay Channel 2 sebagai indikator sistem siap menerima perintah
  digitalWrite(READY_INDICATOR_PIN, isRelayActiveLow ? LOW : HIGH); 

  Serial.println("Sistem Siap. Hubungkan Telegram Anda dengan mengetik /start");
}

void loop() {
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}