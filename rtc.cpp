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
    Serial.print("RTC is NOT running! ");
    Serial.println(rtc.now().unixtime());
    // TODO - this could make a web request to set and continually update the RTC
    rtc_set();
  }
}

void rtc_set() {
  char timestamp[50];
  int i = 0;
  bool timestamp_input = false;
  uint32_t value = 0;

  while (true) {
    Serial.println("Setting RTC, enter the current unix timestamp");
    
    while (Serial.available()) {
      char c = Serial.read();
      timestamp[i++] = c;
      if (c == '\n') { timestamp_input = true; }
    }
    
    if (timestamp_input) {
      value = strtoul(timestamp, NULL, 0);

      // sanity check
      if (value < 1810148331 && value > 1494614996) {
        Serial.println("done setting");
        break;
      } else {
        Serial.println("The value you entered looks wrong - please try again");
        timestamp_input = false;
        i = 0;
      }
    }
    watchdog_feed();
    delay(2000);
  }

  Serial.print("setting timestamp to: ");
  Serial.println(value);
  
  rtc.adjust(DateTime(value));
  update_last_reading_time(0);
}
