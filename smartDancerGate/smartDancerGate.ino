// Full code with melody + gate + alert integration
#include "thingProperties.h"

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Keypad.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define BOT_TOKEN "your_bot_token"
#define CHAT_ID "your_chat_id"
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

const int ledPin = 2;
bool ledState = LOW;
const int redLedPin = 17;
#define BUZZER_PIN 16
#define SERVO_PIN 4
Servo myServo;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'D', 'C', 'B', 'A'},
  {'#', '9', '6', '3'},
  {'0', '8', '5', '2'},
  {'*', '7', '4', '1'}
};
byte rowPins[ROWS] = {14, 27, 26, 25};
byte colPins[COLS] = {33, 32, 13, 12};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String inputPassword = "";

#define RST_PIN 15
#define SS_PIN 5
MFRC522 rfid(SS_PIN, RST_PIN);

struct User {
  String name;
  byte uid[4];
  String telegramPassword;
};

User users[] = {
  {"Ceren", {0x9, 0xA6, 0xF8, 0xB8}, "4578"},
  {"Ela", {0x97, 0xBC, 0x39, 0x63}, "4034"},
  {"Zeynep", {0xD6, 0x69, 0xB3, 0xB4}, "9580"},
  {"Meryem", {0x7E, 0xD1, 0xFB, 0x3}, "7780"}
};
const int numUsers = sizeof(users) / sizeof(users[0]);

unsigned long lastTimeBotRan = 0;
const int botRequestDelay = 1000;

#define NOTE_E5  659
#define NOTE_D5  587
#define NOTE_FS4 370
#define NOTE_GS4 415
#define NOTE_CS5 554
#define NOTE_B4  494
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_A4  440
#define NOTE_CS4 277
#define NOTE_AS4 466
#define NOTE_C5  523
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_F5  698
#define NOTE_DS5 622
#define REST     0

int openMelody[] = {
  REST, 2, NOTE_D4, 4, NOTE_G4, -4, NOTE_AS4, 8, NOTE_A4, 4,
  NOTE_G4, 2, NOTE_D5, 4, NOTE_C5, -2, NOTE_A4, -2,
  NOTE_G4, -4, NOTE_AS4, 8, NOTE_A4, 4, NOTE_F4, 2,
  NOTE_GS4, 4, NOTE_D4, -1, NOTE_D4, 4,
  NOTE_G4, -4, NOTE_AS4, 8, NOTE_A4, 4, NOTE_G4, 2,
  NOTE_D5, 4, NOTE_F5, 2, NOTE_E5, 4, NOTE_DS5, 2, NOTE_B4, 4,
  NOTE_DS5, -4, NOTE_D5, 8, NOTE_CS5, 4, NOTE_CS4, 2,
  NOTE_B4, 4
};

int closeMelody[] = {
  NOTE_E5,8, NOTE_D5,8, NOTE_FS4,4, NOTE_GS4,4,
  NOTE_CS5,8, NOTE_B4,8, NOTE_D4,4, NOTE_E4,4,
  NOTE_B4,8, NOTE_A4,8, NOTE_CS4,4, NOTE_E4,4,
  NOTE_A4,2
};

void showMessage(String line1, String line2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2 != "") {
    display.setCursor(0, 32);
    display.println(line2);
  }
  display.display();
}

void playMelody(int* melody, int length, bool opening) {
  int tempo = 180;
  int wholenote = (60000 * 4) / tempo;
  myServo.detach();
  for (int i = 0; i < length / 2; i++) {
    int note = melody[i * 2];
    int divider = melody[i * 2 + 1];
    int duration = wholenote / abs(divider);
    if (divider < 0) duration = duration * 3 / 2;
    tone(BUZZER_PIN, note, duration * 0.9);
    delay(duration);
    noTone(BUZZER_PIN);
    delay(duration * 0.1);
    int angle = opening ? map(i % 5, 0, 4, 0, 110) : map(i % 5, 0, 4, 110, 0);
    myServo.attach(SERVO_PIN);
    myServo.write(angle);
    delay(30);
    myServo.detach();
  }
  myServo.attach(SERVO_PIN);
  myServo.write(opening ? 110 : 0);
}

void wrongPasswordAlert() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(redLedPin, HIGH);
    digitalWrite(ledPin, HIGH);
    tone(BUZZER_PIN, 900);
    delay(200);
    digitalWrite(redLedPin, LOW);
    digitalWrite(ledPin, LOW);
    noTone(BUZZER_PIN);
    delay(200);
  }
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  digitalWrite(ledPin, ledState);
  Wire.begin(21, 22);
  display.begin(0x3C, true);
  showMessage("Cihaz basliyor...", "Wi-Fi baglantisi...");
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  client.setInsecure();
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  SPI.begin(18, 19, 23, 5);
  rfid.PCD_Init();
  if (!rfid.PCD_PerformSelfTest()) {
    Serial.println("RFID modülü başlatılamadı!");
    showMessage("RFID HATASI", "Modül okunamıyor!");
  } else {
    Serial.println("RFID aktif.");
  }
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  showMessage("Sifreyi girin", "veya kart okutun");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) ArduinoCloud.update();

  char key = keypad.getKey();
  if (key) {
    if (key == '#') {
      bool found = false;
      for (int i = 0; i < numUsers; i++) {
        if (inputPassword == users[i].telegramPassword) {
          String msg = users[i].name + " keypad ile acildi";
          showMessage("Keypad", msg);
          bot.sendMessage(CHAT_ID, "🔓 " + msg, "");
          playMelody(openMelody, sizeof(openMelody)/sizeof(int), true);
          delay(5000);
          playMelody(closeMelody, sizeof(closeMelody)/sizeof(int), false);
          if (users[i].name == "Ceren") cerenCount++;
          else if (users[i].name == "Ela") elaCount++;
          else if (users[i].name == "Zeynep") zeynepCount++;
          else if (users[i].name == "Meryem") meryemCount++;
          found = true;
          break;
        }
      }
      if (!found) {
        showMessage("Hatali sifre");
        bot.sendMessage(CHAT_ID, "❌ Hatali keypad sifresi", "");
        wrongPasswordAlert();
      }
      inputPassword = "";
      delay(1000);
      showMessage("Sifreyi girin");
    } else if (key == '*') {
      inputPassword = "";
      showMessage("Sifre sifirlandi");
      delay(500);
      showMessage("Sifreyi girin");
    } else {
      inputPassword += key;
      showMessage("Girilen:", inputPassword);
    }
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    byte rfidUID[4];
    for (byte i = 0; i < 4; i++) {
      rfidUID[i] = rfid.uid.uidByte[i];
      Serial.print(rfidUID[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    bool authorized = false;
    String userName = "Unknown";
    
    for (int i = 0; i < numUsers; i++) {
      if (memcmp(rfidUID, users[i].uid, 4) == 0) {
        authorized = true;
        userName = users[i].name;
        break;
      }
    }

    if (authorized) {
      String msg = userName + " RFID ile acildi";
      digitalWrite(ledPin, HIGH);
      ledState = HIGH;
      showMessage("RFID OK", msg);
      bot.sendMessage(CHAT_ID, "🔑 " + msg, "");
      
      if (userName == "Ceren") cerenCount++;
      else if (userName == "Ela") elaCount++;
      else if (userName == "Zeynep") zeynepCount++;
      else if (userName == "Meryem") meryemCount++;
      
      playMelody(openMelody, sizeof(openMelody)/sizeof(int), true);
      delay(5000);
      playMelody(closeMelody, sizeof(closeMelody)/sizeof(int), false);
    } else {
      showMessage("Yetkisiz kart");
      bot.sendMessage(CHAT_ID, "🚫 Bilinmeyen RFID kart", "");
      wrongPasswordAlert();
    }

    showMessage("Sifreyi girin", "veya kart okutun");
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  if (WiFi.status() == WL_CONNECTED && millis() - lastTimeBotRan > botRequestDelay) {
    int newMsgs = bot.getUpdates(bot.last_message_received + 1);
    while (newMsgs) {
      for (int i = 0; i < newMsgs; i++) {
        String text = bot.messages[i].text;
        String chat_id = bot.messages[i].chat_id;
        if (text.startsWith("/open ")) {
          String pass = text.substring(6);
          bool found = false;
          for (int j = 0; j < numUsers; j++) {
            if (pass == users[j].telegramPassword) {
              String msg = users[j].name + " Telegram ile acildi";
              bot.sendMessage(chat_id, msg);
              showMessage("Telegram", msg);
              playMelody(openMelody, sizeof(openMelody)/sizeof(int), true);
              delay(5000);
              playMelody(closeMelody, sizeof(closeMelody)/sizeof(int), false);
              if (users[j].name == "Ceren") cerenCount++;
              else if (users[j].name == "Ela") elaCount++;
              else if (users[j].name == "Zeynep") zeynepCount++;
              else if (users[j].name == "Meryem") meryemCount++;
              found = true;
              break;
            }
          }
          if (!found) {
            bot.sendMessage(chat_id, "❌ Hatali sifre", "");
            showMessage("Hatali sifre");
            wrongPasswordAlert();
          }
        } else if (text == "/start") {
          bot.sendMessage(chat_id, "Komutlar:\n/open <sifre>", "");
        }
      }
      newMsgs = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

void onCerenCountChange() {}
void onElaCountChange() {}
void onZeynepCountChange() {}
void onMeryemCountChange() {}
