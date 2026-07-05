#include <Adafruit_GFX.h>     // core graphics library
#include <Adafruit_ST7789.h>  // driver for the ST7789 screen
#include <SPI.h>
#include <OneButton.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include "secret.h"

// --- Pin assignments (change these to match how YOU wired it) ---
#define TFT_CS 7
#define TFT_RST 10
#define TFT_DC 8
#define TFT_SCLK 20
#define TFT_MOSI 21
#define TFT_BL 6

#define BUTTON_PIN 3
#define BUZZER_PIN 5


// --- Objects live out here, outside any function ---
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 21600);  // for my timezone
WebServer server(80);

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

// config settings
int focusMinutes = 25;
int breakMinutes = 5;

// alarm stuff :0
struct Alarm {
  int hour;
  int minute;
  bool active;
};

Alarm alarms[3] = { {0, 0, false}, {0, 0, false}, {0, 0, false} };

void handleRoot() {
  String html = "<h1>Settings:</h1><form action='/set' method='GET'>Focus: <input type='number' name='f' value='" + String(focusMinutes) + "'>m<br>";
  html += "Break: <input type='number' name='b' value='" + String(breakMinutes) + "'>m<br><hr>";
  for(int i=0; i<3; i++) {
    html += "Alarm " + String(i+1) + ": <input type='number' name='h"+String(i)+"' value='"+String(alarms[i].hour)+"' style='width:40px'>:";
    html += "<input type='number' name='m"+String(i)+"' value='"+String(alarms[i].minute)+"' style='width:40px'>";
    html += " Active: <input type='checkbox' name='a"+String(i)+"' " + (alarms[i].active ? "checked" : "") + "><br>";
  }
  html += "<input type='submit' value='Save Settings'></form>";
  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("f")) focusMinutes = server.arg("f").toInt();
  if (server.hasArg("b")) breakMinutes = server.arg("b").toInt();
  for(int i=0; i<3; i++) {
    if (server.hasArg("h"+String(i))) alarms[i].hour = server.arg("h"+String(i)).toInt();
    if (server.hasArg("m"+String(i))) alarms[i].minute = server.arg("m"+String(i)).toInt();
    alarms[i].active = server.hasArg("a"+String(i));
  }
  server.send(200, "text/plain", "Settings Updated!");
}

// setup() runs ONCE when the board powers on
void setup() {
  Serial.begin(115200);  // lets the board talk to your computer

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  tft.init(284, 76);           // screen dimensions
  tft.setRotation(2);          // orientation, could be different, try 0-3
  tft.fillScreen(ST77XX_BLACK);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  timeClient.begin();

  // SERVER setup
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("IP Address: " + WiFi.localIP().toString());

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
      timeRemaining = focusMinutes * 60;
    } else if (currentState == FOCUS) {
      currentState = BREAK;
      timeRemaining = breakMinutes * 60;
    } else {
      currentState = IDLE;
    }

    isPaused = false;
  });
}

// loop() runs OVER and OVER, forever
void loop() {
  btn.tick();
  server.handleClient();
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
  
  if (!alarmActive) {
    for(int i=0; i<3; i++) {
      if (alarms[i].active && alarms[i].hour == timeClient.getHours() && alarms[i].minute == timeClient.getMinutes()) {
        alarmActive = true;
        alarmStartedTime = millis();
        break;
      }
    }
  }

  updateDisplay();
  delay(50);
}

void updateDisplay() {
  if (alarmActive) {
    tft.fillScreen(ST77XX_RED);
    tft.setCursor(20, 20);
    tft.setTextSize(3);
    tft.print("ALARM!!");
    tft.setTextSize(1);
    tft.setCursor(20, 50);
    tft.print("Press to stop");
    return; // Skip drawing the rest
  }

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