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

// alarm pin
#define CLOCK_INTERRUPT_PIN 4

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

// time util
DateTime now, editedNow, editedSleepTime, editedWakeTime;
TimeSpan editTime(0);
char editOrder[10][13]   = {"Year", "Month", "Day", "Hour", "Minute", "Second", "Sleep Hour", "Sleep Minute", "Wake Hour", "Wake Minute"};
unsigned int daysOfMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int editPos = -1;
bool editSecondOnce = false;

// encoder count
volatile int counter = 0;
volatile int direction = 0;
int lastStateCLK;

// button
OneButton button = OneButton(SW_PIN, true, true);

unsigned long systemTime = 0;

#pragma endregion

// alarm
RTC_DATA_ATTR DateTime sleepTime;
RTC_DATA_ATTR DateTime wakeTime;

#pragma region function
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

// draw indicator in edit mode
void drawEditIndicator() {
  unsigned int pos[10][2] = {{45, 32}, {21, 32}, {4, 32},
                             {24, 56}, {60, 56}, {96, 56},
                             {41, 56}, {78, 56}, {41, 56}, {78, 56}};
  display.setCursor(pos[editPos][0], pos[editPos][1]);
  display.setTextSize(1);
  display.print("^");
}

// prevent variable from off value limit, if off then roll to other end
int roll(int currVal, int minVal, int maxVal) {
  if (currVal > maxVal) currVal = minVal;
  if (currVal < minVal) currVal = maxVal;
  return currVal;
}

// limit the value
int limit(int currVal, int minVal, int maxVal) {
  if (currVal > maxVal) currVal = maxVal;
  if (currVal < minVal) currVal = minVal;
  return currVal;
}

// encoder count
void IRAM_ATTR dial() {
    int currentStateCLK = digitalRead(CLK_PIN);
    if (currentStateCLK != lastStateCLK) {
      if (digitalRead(DT_PIN) != currentStateCLK) {
          direction = 1;
      } else {
          direction = -1;
      }
      counter += direction;
      Serial.print(counter); Serial.print(" "); Serial.println(direction);
    
      if (isEditing) {
        switch (editPos) {
          case 0: {// year
            editTime = editTime + (DateTime(limit(editedNow.year() + direction, 2000, 2135), editedNow.month(), editedNow.day(), editedNow.hour(), editedNow.minute(), editedNow.second()) - editedNow);
            break;
          }
          case 1: {// month
            editTime = editTime + (DateTime(editedNow.year(), roll(editedNow.month() + direction, 1, 12), editedNow.day(), editedNow.hour(), editedNow.minute(), editedNow.second()) - editedNow);
            break;
          }
          case 2: {// day
            int month = editedNow.month(), year = editedNow.year();
            int days = daysOfMonth[month - 1];
            if ((month == 2) && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) days = 29;
            editTime = editTime + (DateTime(editedNow.year(), editedNow.month(), roll(editedNow.day() + direction, 1, days), editedNow.hour(), editedNow.minute(), editedNow.second()) - editedNow);
            break;
          }
          case 3: {// hour
            editTime = editTime + (DateTime(editedNow.year(), editedNow.month(), editedNow.day(), roll(editedNow.hour() + direction, 0, 23), editedNow.minute(), editedNow.second()) - editedNow);
            break;
          }
          case 4: {// minute
            editTime = editTime + (DateTime(editedNow.year(), editedNow.month(), editedNow.day(), editedNow.hour(), roll(editedNow.minute() + direction, 0, 59), editedNow.second()) - editedNow);
            break;
          }
          case 5: {// second
            if (!editSecondOnce) {
              editTime = editTime + (DateTime(editedNow.year(), editedNow.month(), editedNow.day(), editedNow.hour(), editedNow.minute(), 60) - editedNow);
              editSecondOnce = true;
            }
            break;
          }
          case 6: {// sleep hour
            editedSleepTime = DateTime(2000, 1, 1, roll(editedSleepTime.hour() + direction, 0, 23), editedSleepTime.minute(), 0);
            break;
          }
          case 7: {// sleep minute
            editedSleepTime = DateTime(2000, 1, 1, editedSleepTime.hour(), roll(editedSleepTime.minute() + direction, 0, 59), 0);
            break;
          }
          case 8: {// wake hour
            editedWakeTime = DateTime(2000, 1, 1, roll(editedWakeTime.hour() + direction, 0, 23), editedWakeTime.minute(), 0);
            break;
          }
          case 9: {// wake minute
            editedWakeTime = DateTime(2000, 1, 1, editedWakeTime.hour(), roll(editedWakeTime.minute() + direction, 0, 59), 0);
            break;
          }
        }
        editedNow = now + editTime;
      }
    }
  lastStateCLK = currentStateCLK;
}

// short press handling
void handleShortPress() {
  if (!isEditing) {
    isAlarming = false;
  } else {
    if (editPos + 1 > 9) {
      editPos = 0;
    } else {
      editPos++;
    }
    if (editPos == 4 || editPos == 6) {
      editSecondOnce = false;
    }
    counter = 0;
    systemTime = millis();
  }
}

// long press handling
void handleLongPress() {
  // switch to edit mode
  if (!isEditing) {
    isEditing = true;
    editPos = 0;
    editedSleepTime = sleepTime;
    editedWakeTime = wakeTime;
    counter = 0;
  }
  else {
    isEditing = false;
    // save change
    rtc.adjust(editedNow);
    sleepTime = editedSleepTime;
    wakeTime = editedWakeTime;
    rtc.setAlarm1(sleepTime, DS3231_A1_Hour);
    rtc.setAlarm2(wakeTime, DS3231_A2_Hour);
    // reset change
    editTime = TimeSpan(0);
    editPos = -1;
    counter = 0;
  }
  systemTime = millis();
}

// handle double click
void handleDoubleClick() {
  if (isEditing) {
    isEditing = false;
    // reset change
    editTime = TimeSpan(0);
    editedSleepTime = sleepTime;
    editedWakeTime = wakeTime;
    editPos = -1;
    counter = 0;
  }
}
#pragma endregion

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
  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF);
  if (rtc.lostPower()) {
    Serial.println("reset time because rtc lost power");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  sleepTime = rtc.getAlarm1();
  wakeTime = rtc.getAlarm2();
  
  // buzzer
  pinMode(2, OUTPUT);

  // knob encoder
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), dial, CHANGE);
  
  // button
  button.attachClick(handleShortPress);
  button.attachLongPressStart(handleLongPress);
  button.attachDoubleClick(handleDoubleClick);
  button.setPressMs(1000);

  systemTime = millis();
}

void loop() {
  now = rtc.now();
  editedNow = now + editTime;

  #pragma region display
  display.clearDisplay();
  display.setCursor(0, 6);
  // bruh put this line in here or time toString not work
  char dateFormat[] = "DD-MM-YYYY", timeFormat[] = "hh:mm:ss", alarmFormat[] = "hh:mm";
  if (editPos < 6) {
    // complete time string
    String date = editedNow.toString(dateFormat);
    String time = editedNow.toString(timeFormat);
    String dayOfWeek = daysOfTheWeek[editedNow.dayOfTheWeek()];

    // editing status or weekday
    if (!isEditing) {
      println(dayOfWeek + "\n");
    } else {
      String order = editOrder[editPos];
      println("Editing: " + order + "\n");
    }
    // show date
    if (!isEditing || (editPos >= 0 && editPos <= 2)) {
      println(date + "\n"); 
    } else {
      println("\n");
    }
    // show time
    if (!isEditing || (editPos >= 3 && editPos <= 5)) {
      println(time, 2, CENTER);
    } else {
      println("\n");
    }
  } else {
    String alarmTime;
    if (editPos == 6 || editPos == 7) { // sleep
      alarmTime = editedSleepTime.toString(alarmFormat);
    } else { // wake
      alarmTime = editedWakeTime.toString(alarmFormat);
    }
    String order = editOrder[editPos];
    println("Editing: " + order + "\n");
    println("\n");
    println(alarmTime, 2, CENTER);
  }

  if (!isEditing) {
    // tempurature
    display.setCursor(0, 6);
    println(String(rtc.getTemperature(), 1) + " C", 1, RIGHT);
    display.drawCircle(116, 8, 2, SH110X_WHITE);
  } 
  #pragma endregion

  // alarm
  if (rtc.alarmFired(1)) { // sleep alarm
    rtc.clearAlarm(1);
    isLocking = true;
    systemTime = millis();
  } else if (rtc.alarmFired(2)) { // wake alarm
    rtc.clearAlarm(2);
    isLocking = false;
    isAlarming = true;
  }

  // buzzer manage
  if (isAlarming) {
    // create interrupted buzzer effect
    if (millis() - systemTime <= 200) {
      digitalWrite(2, HIGH);
    } else {
      digitalWrite(2, LOW);
      systemTime = millis();
    }
  } else {
    // buzzing when entering locking mode or switch in edit mode
    // disable buzzer after .1 sec from locking
    if (millis() - systemTime >= 100) digitalWrite(2, LOW);
    else digitalWrite(2, HIGH);
  } 
  
  // set current time using sync-time.py
  if (Serial.available()) {
    DateTime resetTime = DateTime(Serial.parseInt(), Serial.parseInt(), Serial.parseInt(), Serial.parseInt(), Serial.parseInt(), Serial.parseInt());
    rtc.adjust(resetTime);
    Serial.println("ok");
  } 

  if (isEditing) drawEditIndicator();
  drawStatusIcons();
  display.display();
  button.tick();
}