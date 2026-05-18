#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Wifi network station credentials
#define WIFI_SSID "MIKRO"
#define WIFI_PASSWORD "1DEAlist"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "8781341537:AAFa3l_WPome0qOHzt9X73emdHGAT3oIBsM"

const unsigned long BOT_MTBS = 1000;  // mean time between scan messages

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime;  // last time messages' scan has been done

// --- KONFIGURASI RELAY ---
const int RELAY_PIN = 26;  // Menggunakan IO12 pada ESP32 S2 Mini (silakan sesuaikan jika perlu)

// Ubah ke false jika relay Anda jenis Active High (menyala saat HIGH)
bool isRelayActiveLow = true;

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

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
      bot.sendMessage(chat_id, "Relay berhasil dinyalakan (ON).", "");
    }

    // Perintah untuk mematikan relay
    else if (text == "/relay_off") {
      digitalWrite(RELAY_PIN, isRelayActiveLow ? HIGH : LOW);
      bot.sendMessage(chat_id, "Relay berhasil dimatikan (OFF).", "");
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

  // Menunggu sampai Serial Monitor benar-benar terbuka & siap
  while (!Serial) {
    delay(10);
  }
  Serial.println();

  // Inisialisasi pin Relay sebagai OUTPUT
  pinMode(RELAY_PIN, OUTPUT);
  // Kondisi awal: pastikan relay dalam keadaan mati (OFF) saat dinyalakan
  digitalWrite(RELAY_PIN, isRelayActiveLow ? HIGH : LOW);

  // attempt to connect to Wifi network:
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

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org");  // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
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