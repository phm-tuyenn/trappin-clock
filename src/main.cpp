#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <RTClib.h>
#include <OneButton.h>
#include <math.h>

#include "icon.h"

#pragma region variables
// encoder pin
#define CLK_PIN 25
#define DT_PIN 26
#define SW_PIN 27

// oled config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// rtc config
RTC_DS3231 rtc;

// date & format
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
enum ALIGN {LEFT, CENTER, RIGHT};

// status
bool isAlarming = false;
bool isEditing = false;
bool isLocking = false;

// alarm
RTC_DATA_ATTR DateTime sleepTime = DateTime(2026, 7, 7, 8, 0, 0);
RTC_DATA_ATTR DateTime wakeTime = DateTime(2026, 7, 7, 8, 0, 0);
DateTime now;

// encoder count
volatile int counter = 0;
int lastStateCLK;

// button
OneButton button = OneButton(SW_PIN, true, true);

unsigned long systemTime = 0;

#pragma endregion

// oled display print()
void print(String text = "", uint8_t size = 1, ALIGN align = LEFT) {
  display.setTextSize(size);
  int xOffset;
  switch (align) {
  case LEFT:
    xOffset = 0;
    break;
  
  case CENTER:
    xOffset = (132 - 6 * size * text.length()) / 2 + 3;
    break;

  case RIGHT:
    xOffset = 132 - 6 * size * text.length();
    break;
  } 
  display.setCursor(xOffset, display.getCursorY());
  display.print(text);
}

// oled display println()
void println(String text = "", uint8_t size = 1, ALIGN align = LEFT) {
  print(text + "\n", size, align);
}

// draw status icons
void drawStatusIcons() {
  if (isLocking) {
    display.drawBitmap(114, 18, lockIcon, 12, 12, SH110X_WHITE);
  } else {
    display.drawBitmap(114, 18, unlockIcon, 12, 12, SH110X_WHITE);
  }

  if (isAlarming) {
    display.drawBitmap(100, 18, alarmIcon, 12, 12, SH110X_WHITE);
  }

  if (isEditing) {
    if (!isAlarming) display.drawBitmap(100, 18, editIcon, 12, 12, SH110X_WHITE);
    else display.drawBitmap(86, 18, editIcon, 12, 12, SH110X_WHITE);
  }
}

// encoder count
void IRAM_ATTR rotary_encoder() {
    int currentStateCLK = digitalRead(CLK_PIN);
    if (currentStateCLK != lastStateCLK) {
        if (digitalRead(DT_PIN) != currentStateCLK) {
            counter++;
        } else {
            counter--;
        }
    }
    lastStateCLK = currentStateCLK;
}

// short press handling
void handleShortPress() {
  if (!isEditing) {
    isAlarming = false;
  } else {
    // switch to another term to edit
  }
}

// long press handling
void handleLongPress() {
  // switch to edit mode
  isEditing = !isEditing;
}

void setup() {
  Serial.begin(115200);

  // oled init
  Wire1.begin(18, 19);
  while (!display.begin(0x3C, true)) { 
    Serial.println("oled not found");
    delay(2000);
  }
  display.setTextColor(SH110X_WHITE);

  // rtc init
  while (!rtc.begin()) {
    Serial.println("rtc not found");
    delay(2000);
  }

  if (rtc.lostPower()) {
    Serial.println("reset time because rtc lost power");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // buzzer
  pinMode(2, OUTPUT);

  // knob encoder
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), rotary_encoder, CHANGE);
  
  // button
  button.attachClick(handleShortPress);
  button.attachLongPressStart(handleLongPress);
  button.setPressMs(1000);

  systemTime = millis();
}

void loop() {
  now = rtc.now();
  // getting each time field in individual variables
  // adding a leading zero when needed
  String yearStr = String(now.year(), DEC);
  String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  String hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC); 
  String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
  String secondStr = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
  String dayOfWeek = daysOfTheWeek[now.dayOfTheWeek()];

  // complete time string
  String date = dayStr + "-" + monthStr + "-" + yearStr;
  String time = hourStr + ":" + minuteStr + ":" + secondStr;

  // print time
  display.clearDisplay();
  display.setCursor(0, 6);

  println(dayOfWeek + "\n");
  println(date + "\n");

  println(time, 2, CENTER);

  // tempurature
  display.setCursor(0, 6);
  println(String(rtc.getTemperature(), 1) + " C", 1, RIGHT);
  display.drawCircle(116, 8, 2, SH110X_WHITE);

  // alarm toggle
  if (now.hour() == wakeTime.hour() && now.minute() == wakeTime.minute() && now.second() == 0) {
    isAlarming = true;
  }
  // create interrupted buzzer effect
  if (millis() - systemTime <= 200) 
    digitalWrite(2, isAlarming);
  else {
    digitalWrite(2, LOW);
    systemTime = millis();
  }
  
  // set current time using sync-time.py
  if (Serial.available()) {
    DateTime resetTime = DateTime(Serial.parseInt(), Serial.parseInt(), Serial.parseInt(), Serial.parseInt(), Serial.parseInt(), Serial.parseInt());
    rtc.adjust(resetTime);
    Serial.println("ok");
  } 

  drawStatusIcons();
  display.display();
  button.tick();
}