#include <Adafruit_GFX.h>     // core graphics library
#include <Adafruit_ST7789.h>  // driver for the ST7789 screen
#include <SPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secret.ino"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 21600);  // for my timezone

String alarmTime = "21:30:00" bool alarmTriggered = false;

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

// setup() runs ONCE when the board powers on
void setup() {
  Serial.begin(115200);  // lets the board talk to your computer

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  tft.init(284, 76);           // screen dimensions
  tft.setColRowStart(82, 18);  // offsets to line up this unusual panel
  tft.setRotation(2);          // orientation, could be different, try 0-3

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  timeClient.begin();
  tft.fillScreen(ST77XX_BLACK);
}

// loop() runs OVER and OVER, forever
void loop() {
  timeClient.update();
  String currentTime = timeClient.getFormattedTime();
  tft.fillRect(0, 0, 284, 76, ST77XX_BLACK);
  tft.setCursor(10, 20);
  tft.setTextSize(4);
  tft.print(currentTime);

  if (currentTime == alarmTime) {
    alarmTriggered = true;
  }

  if (alarmTriggered) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      alarmTriggered = false;
      digitalWrite(BUZZER_PIN, LOW);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  delay(1000);
}