#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>


#define GAME_NAME "Tebak Jarak"
#define AP_SETUP "HIMATIKOM (Tebak Jarak)"
#define APPS_SCRIPT_URL "https://script.google.com/macros/s/AKfycbwc-W8LzhZEsySWBSY-skmOaZ-173OLcsLg1PgGqsJA23L9GlahC6ykBbMG0t966MuhWw/exec"

// --- VARIABEL GLOBAL DATA PEMAIN ---
String playerName = "";
String playerWA = "";          // Nomor WhatsApp (hanya untuk penyimpanan)
String playerTelegramID = "";  // Username Telegram (jika ada)

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "8529296741:AAHv0KwoZN7teUl9TLvLYhCvwathTG7oyhU"

String CHAT_ID = "7917806728";  // Ganti dengan Chat ID pengguna (tempat bot akan mengirim pesan)

// --- KONFIGURASI PIN & GAME ---
const int trigPin = D5;         // Sesuaikan pin jika perlu (misalnya D1 = GPIO5, D2 = GPIO4, dst.)
const int echoPin = D6;         // Sesuaikan pin jika perlu
const long MAX_DISTANCE = 200;  // Batas jarak maksimum (cm)
const long MIN_DISTANCE = 50;
const long GAME_TIME = 10000;  // Waktu permainan (10 detik)

// Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Sesuaikan alamat jika perlu (0x27/0x3F)

// --- VARIABEL GLOBAL ---
WiFiClientSecure securedClient;
UniversalTelegramBot bot(BOT_TOKEN, securedClient);
WiFiManager wm;

bool gameActive = false;
long targetDistance = 0;
long actualDistance = 0;
long duration;

long lastTimeBotUpdate = 0;
const int botRequestDelay = 1000;  // Cek bot setiap 1 detik

// --- PROTOTYPE FUNGSI ---
void handleNewMessages(int numNewMessages);
long getDistance();
void startGame();
void standbyDisplay();

// =========================================================================
// FUNGSI SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();

  // Konfigurasi pin Ultrasonik
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Seed random generator (menggunakan waktu boot, kurang acak tapi lebih baik dari A0 di ESP)
  randomSeed(micros());

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Game Tebak");
  lcd.setCursor(5, 1);
  lcd.print("Jarak");

  delay(5000);

  // Hubungkan ke Wi-Fi
  lcd.clear();
  lcd.print("Set Up AP:");
  lcd.setCursor(0, 1);
  lcd.print(AP_SETUP);

  bool res;
  res = wm.autoConnect(AP_SETUP);  // password protected ap
  securedClient.setInsecure();     // Nonaktifkan verifikasi SSL untuk kemudahan (hati-hati)

  if (!res) {
    Serial.println("Failed to connect");
    lcd.clear();
    lcd.print("Wifi Not Found");
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    lcd.clear();
    lcd.print("Connected to:");
    lcd.setCursor(0, 1);
    lcd.print(wm.getWiFiSSID());
  }

  delay(3000);

  // Tampilkan pesan standby
  standbyDisplay();
}

// =========================================================================
// FUNGSI PENGUKURAN JARAK
// =========================================================================
long getDistance() {
  // Mirip dengan kode asli, tetapi sesuaikan timing ESP8266
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);

  // Hitung jarak (cm)
  long distanceCm = duration * 0.034 / 2;

  if (distanceCm > MAX_DISTANCE) {
    distanceCm = MAX_DISTANCE + 1;
  }

  return distanceCm;
}

// =========================================================================
// FUNGSI TAMPILAN STANDBY
// =========================================================================
void standbyDisplay() {
  lcd.clear();
  lcd.print("Kirim /main ke:");
  lcd.setCursor(0, 1);
  lcd.print("@Himatikom_bot");  // Ganti dengan nama pengguna Bot Anda
}

// =========================================================================
// FUNGSI MEMULAI GAME
// =========================================================================
void startGame() {
  gameActive = true;

  // 1. Tentukan Jarak Target
  targetDistance = (long)random(MIN_DISTANCE, MAX_DISTANCE + 1);

  lcd.clear();
  lcd.print("Berdiri Jarak:");
  lcd.setCursor(0, 1);
  lcd.print(targetDistance);
  lcd.print(" cm");

  // Kirim target ke Telegram (PENTING: JANGAN LAKUKAN INI jika target harus rahasia)
  // Di sini, kita asumsikan target ditampilkan di LCD dan objek harus diletakkan.
  bot.sendMessage(CHAT_ID, "Permainan dimulai! \nBerdirilah di Jarak: " + String(targetDistance) + " cm. \nAnda punya 10 detik. Siap-siap!", "");

  Serial.print("Target:     ");
  Serial.println(targetDistance);

  // 2. Fase Permainan
  unsigned long startTime = millis();
  unsigned long timeLeft = GAME_TIME;

  lcd.setCursor(0, 0);
  lcd.print("Target:     ");
  lcd.setCursor(10, 0);
  lcd.print("Waktu");

  // Loop selama waktu permainan belum habis
  while (millis() - startTime < GAME_TIME) {
    // Tampilkan sisa waktu setiap 1 detik
    if ((GAME_TIME - (millis() - startTime)) < (timeLeft - 1000) || (timeLeft == GAME_TIME)) {
      timeLeft = GAME_TIME - (millis() - startTime);

      lcd.setCursor(11, 1);
      int secondsLeft = timeLeft / 1000 + 1;  // +1 untuk membulatkan ke atas/terakhir
      if (secondsLeft < 10) lcd.print(" ");
      lcd.print(secondsLeft);
      lcd.print("s");
    }
    // Cek pesan Telegram selama fase permainan (opsional)
    if (millis() > lastTimeBotUpdate + botRequestDelay) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      while (numNewMessages) {
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTimeBotUpdate = millis();
    }
  }

  // 3. Fase Penilaian
  actualDistance = getDistance();
  long difference = abs(targetDistance - actualDistance);

  lcd.clear();
  lcd.print("Anda: ");
  lcd.print(actualDistance);
  lcd.print(" cm");

  // 4. Kirim Hasil ke Telegram
  String resultMessage = "Waktu Habis!\n";
  resultMessage += "Target Jarak: " + String(targetDistance) + " cm\n";

  if (actualDistance > MAX_DISTANCE) {
    resultMessage += "Objek Terlalu Jauh atau tidak terdeteksi.\n";
    lcd.setCursor(0, 1);
    lcd.print("Terlalu Jauh!");
    difference = -1;  // Nilai khusus untuk jarak terlalu jauh
  } else {
    resultMessage += "Jarak Aktual: " + String(actualDistance) + " cm\n";
    resultMessage += "SELISIH (Skor): " + String(difference) + " cm\n";
    lcd.setCursor(0, 1);
    lcd.print("Selisih: ");
    lcd.print(difference);
    lcd.print(" cm");
  }

  bot.sendMessage(CHAT_ID, resultMessage, "");

  Serial.println(resultMessage);

  // 5. Kirim data ke Spreadsheet
  sendDataToSpreadsheet(difference);

  delay(5000);         // Tahan tampilan hasil
  gameActive = false;  // Akhiri game

  // 6. Reset Variabel Pemain untuk game berikutnya
  playerName = "";
  playerWA = "";
  playerTelegramID = "";

  // Kembali ke tampilan standby
  standbyDisplay();
}

// =========================================================================
// FUNGSI PENANGANAN PESAN BOT
// =========================================================================
// FUNGSI PENANGANAN PESAN BOT
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    String from_id = bot.messages[i].from_id;
    String from_name = bot.messages[i].from_name;
    String username = bot.messages[i].from_id;  // Username Telegram

    // 1. Perintah START
    if (text == "/start") {
      String welcome = "Halo " + from_name + "! Saya adalah Bot Game Tebak Jarak. Kirim /main untuk memulai game.";
      bot.sendMessage(chat_id, welcome, "");
    }

    // 2. Perintah MAIN (Memulai Input)
    else if (text == "/main" && !gameActive) {
      CHAT_ID = chat_id;            // Simpan chat ID saat ini
      playerTelegramID = username;  // Simpan Telegram ID (username)

      // Kirim Keyboard Kustom untuk memandu input
      String keyboardJson = "[[\"BATAL\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id,
                                       "**Siap memulai!**\n\nMasukkan **Nama Lengkap Anda**:",
                                       "", keyboardJson, true);

    }

    // 3. Fase Input Nama
    else if (text != "/main" && !gameActive && playerName.isEmpty() && chat_id == CHAT_ID) {
      if (text == "BATAL") {
        playerName = "";  // Reset
        bot.sendMessage(chat_id, "Permintaan dibatalkan. Kirim /main untuk memulai lagi.", "", true);
        return;
      }
      playerName = text;

      // Minta input WA
      String keyboardJson = "[[\"BATAL\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id,
                                       "Terima kasih, *" + playerName + "*. Sekarang masukkan **Nomor WhatsApp** Anda (Contoh: 081234567890):",
                                       "", keyboardJson, true);
    }

    // 4. Fase Input WA & Mulai Game
    else if (text != "/main" && !gameActive && !playerName.isEmpty() && playerWA.isEmpty() && chat_id == CHAT_ID) {
      if (text == "BATAL") {
        playerName = "";  // Reset
        playerWA = "";
        bot.sendMessage(chat_id, "Permintaan dibatalkan. Kirim /main untuk memulai lagi.", "");
        return;
      }
      playerWA = text;

      // Reset keyboard setelah input selesai
      bot.sendMessage(chat_id, "Data pemain tersimpan! Memulai permainan tebak jarak...", "");

      startGame();

    }

    // 5. Game Sedang Berlangsung
    else if (gameActive) {
      bot.sendMessage(chat_id, "Permainan sedang berlangsung! Mohon tunggu skor Anda keluar.", "");
    }
  }
}

// =========================================================================
// FUNGSI PENGIRIMAN DATA KE GOOGLE APPS SCRIPT
// =========================================================================
void sendDataToSpreadsheet(long diff) {
  if (diff == -1) {
    // Jangan kirim jika terlalu jauh
    Serial.println("Tidak mengirim data karena jarak terlalu jauh.");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Gagal mengirim data: WiFi terputus.");
    return;
  }
  
  HTTPClient http;
  

  // Siapkan data dalam format JSON
  String payload = "{";
  // 1. Tambahkan parameter GAME_NAME
  payload += "\"game\":\"" + String(GAME_NAME) + "\",";
  // 2. Tambahkan data pemain dan skor
  payload += "\"nama\":\"" + playerName + "\",";
  payload += "\"wa\":\"" + playerWA + "\",";
  payload += "\"skor\":\"" + String(diff) + "\",";
  payload += "\"sosmed1id\":\"" + playerTelegramID + "\",";
  payload += "\"sosmed2id\":\"" + CHAT_ID + "\"}";

  Serial.print("Mengirim data ke Apps Script: ");
  Serial.println(payload);

  http.begin(securedClient, APPS_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    Serial.print("Apps Script Response: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println(response);
    bot.sendMessage(CHAT_ID, "**Skor berhasil disimpan** ke *leaderboard*!", "");
  } else {
    Serial.print("Error saat mengirim data: ");
    Serial.println(httpResponseCode);
    bot.sendMessage(CHAT_ID, "**Gagal menyimpan skor** ke *leaderboard* (Error: " + String(httpResponseCode) + ").", "");
  }

  http.end();
}

// =========================================================================
// FUNGSI LOOP
// =========================================================================
void loop() {
  // Cek pesan Telegram secara berkala
  if (millis() > lastTimeBotUpdate + botRequestDelay) {
    // Game hanya dimulai melalui Telegram, jadi kita hanya perlu cek pesan di loop
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotUpdate = millis();
  }
}