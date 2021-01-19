#ifndef TRANSMIT_H
#define TRANSMIT_H

#define CODE_VERSION "F-1.3.2"

#include "user_config.h"

#ifdef HEATSEEK_FEATHER_CELL_M0
  #define TRANSMITTER_GSM
#else
  #define TRANSMITTER_WIFI
#endif

#ifdef HEATSEEK_FEATHER_WIFI_WICED
  #include <libmaple/iwdg.h>
  #include <adafruit_feather.h>
  #include <adafruit_http.h>

  #define DHT_DATA  PC2
  #define SD_CS     PB4
  
  #define TRANSMITS_PER_LOOP 20
#endif

#ifdef HEATSEEK_FEATHER_WIFI_M0
  #include <ArduinoHttpClient.h>
  #include <WiFi101.h>

  #define DHT_DATA  A2
  #define SD_CS     10
  
  #define TRANSMITS_PER_LOOP 20
#endif

#ifdef TRANSMITTER_GSM
  #include "Adafruit_FONA.h"
  #include <Adafruit_SleepyDog.h>

  #define DHT_DATA  A2
  #define SD_CS     10
  #define FONA_RST  A4
  #define LORA_CS   8
  
  #define TRANSMITS_PER_LOOP 5
#endif

#define SEND_SAVED_READINGS_THRESHOLD (10 * 60)
#define USER_AGENT_HEADER  "curl/7.45.0"
#define PORT               80

void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time);
void transmit_queued_temps();
void clear_queued_transmissions();
#ifdef TRANSMITTER_WIFI
void force_wifi_reconnect();
#endif
  
#endif
