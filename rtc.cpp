#include "rtc.h"
#include "watchdog.h"
#include "config.h"

RTC_PCF8523 rtc;

void rtc_initialize() {
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (true); // watchdog will reboot
  }
  
  if (!rtc.initialized()) {
    Serial.println("RTC is NOT running!");
    // TODO - this could make a web request to set and continually update the RTC
    rtc_set();
  }
}

void rtc_set() {
  char timestamp[50];
  int i = 0;
  bool timestamp_input = false;

  while (true) {
    Serial.println("Setting RTC, enter the current unix timestamp");
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        Serial.println("done setting");
        timestamp_input = true;
        break;
      } else {
        timestamp[i++] = c;
      }
    }
    if (timestamp_input) break;
    watchdog_feed();
    delay(2000);
  }

  Serial.print("setting timestamp to: ");
  Serial.println(strtoul(timestamp, NULL, 0));
  
  rtc.adjust(DateTime(strtoul(timestamp, NULL, 0)));
  update_last_reading_time(0);
}
