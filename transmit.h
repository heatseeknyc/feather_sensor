#ifndef TRANSMIT_H
#define TRANSMIT_H

#ifdef MCU_STM32F205RG
  #define TRANSMITTER_WIFI
#else
  #define TRANSMITTER_GSM
#endif

#ifdef TRANSMITTER_WIFI
  #include <libmaple/iwdg.h>
  #include <adafruit_feather.h>
  #include <adafruit_http.h>

  #define DHT_DATA  PC2
  #define SD_CS     PB4
#endif

#ifdef TRANSMITTER_GSM
  #include "Adafruit_FONA.h"
  #include <Adafruit_SleepyDog.h>

  #define DHT_DATA  A2
  #define SD_CS     10
  #define FONA_RST  A4
  #define LORA_CS   8
#endif

#define USER_AGENT_HEADER  "curl/7.45.0"
#define PORT               80

void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time);

#endif
