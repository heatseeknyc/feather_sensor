#include "watchdog.h"

#if defined(HEATSEEK_FEATHER_WIFI_M0) || defined(TRANSMITTER_GSM)
  #include <Adafruit_SleepyDog.h>
#endif

void watchdog_init() {
  #ifdef HEATSEEK_FEATHER_WIFI_WICED
    iwdg_init(IWDG_PRE_256, 3500); // 30 second watchdog, 40kHz processor
  #endif

  #if defined(HEATSEEK_FEATHER_WIFI_M0) || defined(TRANSMITTER_GSM)
    Watchdog.enable(16000);  // 16 seconds (this is the max supported)
  #endif
}

void watchdog_feed() {
  #ifdef HEATSEEK_FEATHER_WIFI_WICED
    iwdg_feed();
  #endif
  
  #if defined(HEATSEEK_FEATHER_WIFI_M0) || defined(TRANSMITTER_GSM)
    Watchdog.reset();
  #endif
}
