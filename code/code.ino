#include <Adafruit_GFX.h>     // core graphics library
#include <Adafruit_ST7789.h>  // driver for the ST7789 screen
#include <SPI.h>
#include <OneButton.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secret.ino"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 21600);  // for my timezone

// --- Pin assignments (change these to match how YOU wired it) ---
#define TFT_CS 7
#define TFT_RST 10
#define TFT_DC 8
#define TFT_SCLK 20
#define TFT_MOSI 21
#define TFT_BL 6

// --- Objects live out here, outside any function ---
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

#define BUTTON_PIN 3
#define BUZZER_PIN 5

OneButton btn(BUTTON_PIN, true);

enum State { IDLE,
             FOCUS,
             BREAK };
State currentState = IDLE;
unsigned long timeRemaining = 0;
unsigned long lastTick = 0;
unsigned long alarmStartedTime = 0;
bool isPaused = false;
bool alarmActive = false;

// setup() runs ONCE when the board powers on
void setup() {
  Serial.begin(115200);  // lets the board talk to your computer

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  tft.init(284, 76);           // screen dimensions
  tft.setColRowStart(82, 18);  // offsets to line up this unusual panel
  tft.setRotation(2);          // orientation, could be different, try 0-3
  tft.fillScreen(ST77XX_BLACK);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  timeClient.begin();


  btn.attachClick([]() {
    if (alarmActive) {
      alarmActive = false;
      digitalWrite(BUZZER_PIN, LOW);
      currentState = IDLE;
    } else if (currentState != IDLE) {
      isPaused = !isPaused;
    }
  });

  btn.attachLongPressStart([]() {
    if (alarmActive) return;
    if (currentState == IDLE) {
      currentState = FOCUS;
      timeRemaining = 25 * 60;
    } else if (currentState == FOCUS) {
      currentState = BREAK;
      timeRemaining = 5 * 60;
    } else {
      currentState = IDLE;
    }

    isPaused = false;
  });
}

// loop() runs OVER and OVER, forever
void loop() {
  btn.tick();
  timeClient.update();

  if (currentState != IDLE && !isPaused && !alarmActive) {
    if (millis() - lastTick >= 1000) {
      lastTick = millis();
      if (timeRemaining > 0) {
        timeRemaining--;
      } else {
        alarmActive = true;
        alarmStartedTime = millis();
      }
    }
  }
  if (alarmActive) {
    if (millis() - alarmStartedTime < 60000) {
      digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
    }
  }
  updateDisplay();
}

void updateDisplay() {
  tft.fillRect(0, 0, 284, 76, ST77XX_BLACK);
  tft.setCursor(10, 20);
  tft.setTextSize(2);
  tft.print(currentState == IDLE ? "MODE: IDLE" : (currentState == FOCUS ? "MODE: FOCUS" : "MODE: BREAK"));

  if (currentState == IDLE) {
    tft.setCursor(20, 30);
    tft.setTextSize(4);
    tft.print(timeClient.getFormattedTime());
  } else {
    tft.setCursor(20, 30);
    tft.setTextSize(4);
    tft.print(timeRemaining / 60);
    tft.print(":");

    if (timeRemaining % 60 < 10) {
      tft.print("0");
    }

    tft.print(timeRemaining % 60);

    tft.setCursor(10, 65);
    tft.setTextSize(1);
    tft.print("Time: ");
    tft.print(timeClient.getFormattedTime());
  }
}