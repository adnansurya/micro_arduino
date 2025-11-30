#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <LiquidCrystal_I2C.h>

// Wifi network station credentials
#define WIFI_SSID "WARKOP LATEMMAMALA"
#define WIFI_PASSWORD "OKTOBER10"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "8529296741:AAHv0KwoZN7teUl9TLvLYhCvwathTG7oyhU"

String CHAT_ID = "7917806728";  // Ganti dengan Chat ID pengguna (tempat bot akan mengirim pesan)

// --- KONFIGURASI PIN & GAME ---
const int trigPin = D5;        // Sesuaikan pin jika perlu (misalnya D1 = GPIO5, D2 = GPIO4, dst.)
const int echoPin = D6;       // Sesuaikan pin jika perlu
const long MAX_DISTANCE = 200;  // Batas jarak maksimum (cm)
const long GAME_TIME = 10000;  // Waktu permainan (10 detik)

// Inisialisasi LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Sesuaikan alamat jika perlu (0x27/0x3F)

// --- VARIABEL GLOBAL ---
WiFiClientSecure securedClient;
UniversalTelegramBot bot(BOT_TOKEN, securedClient);

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

  // Hubungkan ke Wi-Fi
  lcd.clear();
  lcd.print("Menghubungkan...");
  Serial.print("Menghubungkan ke WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  securedClient.setInsecure();  // Nonaktifkan verifikasi SSL untuk kemudahan (hati-hati)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
  }

  Serial.println("");
  Serial.print("Terhubung! Alamat IP: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.print("WiFi Terhubung!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(2000);

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
  targetDistance = (long)random(1, MAX_DISTANCE + 1);

  lcd.clear();
  lcd.print("Target Jarak:");
  lcd.setCursor(0, 1);
  lcd.print(targetDistance);
  lcd.print(" cm");

  // Kirim target ke Telegram (PENTING: JANGAN LAKUKAN INI jika target harus rahasia)
  // Di sini, kita asumsikan target ditampilkan di LCD dan objek harus diletakkan.
  bot.sendMessage(CHAT_ID, "Permainan dimulai! Target: " + String(targetDistance) + " cm. Anda punya 10 detik. Siap-siap!", "");

  Serial.print("Target: ");
  Serial.println(targetDistance);

  // 2. Fase Permainan
  unsigned long startTime = millis();
  unsigned long timeLeft = GAME_TIME;

  lcd.setCursor(10, 0);
  lcd.print("Waktu:");

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
  lcd.print("T: ");
  lcd.print(targetDistance);
  lcd.print(" A: ");
  lcd.print(actualDistance);

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
  }

  bot.sendMessage(CHAT_ID, resultMessage, "");

  Serial.println(resultMessage);

  delay(5000);         // Tahan tampilan hasil
  gameActive = false;  // Akhiri game

  // Kembali ke tampilan standby
  standbyDisplay();
}

// =========================================================================
// FUNGSI PENANGANAN PESAN BOT
// =========================================================================
void handleNewMessages(int numNewMessages) {
  Serial.print("Ada ");
  Serial.print(numNewMessages);
  Serial.println(" pesan baru");

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    Serial.println(text);

    if (text == "/main" && !gameActive) {
      CHAT_ID = chat_id;
      bot.sendMessage(chat_id, "Memulai permainan tebak jarak...", "");
      startGame();
    } else if (text == "/main" && gameActive) {
      bot.sendMessage(chat_id, "Permainan sedang berlangsung! Mohon tunggu.", "");
    } else if (text == "/start") {
      String welcome = "Halo! Saya adalah Bot Game Tebak Jarak. Kirim /main untuk memulai game.";
      bot.sendMessage(chat_id, welcome, "");
    }
  }
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