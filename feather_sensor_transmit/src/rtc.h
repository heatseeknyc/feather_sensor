#ifndef RTC_H
#define RTC_H

#include <RTClib.h>

extern RTC_PCF8523 rtc;

void rtc_initialize();
void rtc_set();

#endif
