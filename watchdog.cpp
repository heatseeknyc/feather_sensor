#include "watchdog.h"
#include "transmit.h"

void watchdog_init() {
  #ifdef TRANSMITTER_WIFI
    iwdg_init(IWDG_PRE_256, 1476); // 9 second watchdog, 40kHz processor
  #endif

  #ifdef TRANSMITTER_GSM
    Watchdog.enable(8000);
  #endif
}

void watchdog_feed() {
  #ifdef TRANSMITTER_WIFI
    iwdg_feed();
  #endif
  
  #ifdef TRANSMITTER_GSM
    Watchdog.reset();
  #endif
}
