#include <Arduino.h>
#include "config.h"
#include <SD.h>
#include "watchdog.h"
#include "rtc.h"

CONFIG_union CONFIG;

void write_config() {
  File config_file;
  
  if (config_file = SD.open("config.bin", O_WRITE | O_CREAT | O_TRUNC)) {
    config_file.write(CONFIG.raw, sizeof(CONFIG));
    config_file.close();
    //Serial.println("updated config");
  } else {
    Serial.println("unable to update config");
    while(true); // watchdog will reboot
  }
}

bool read_config() {
  File config_file;
  bool success = false;

  if (config_file = SD.open("config.bin", FILE_READ)) {
    int read_size = config_file.read(CONFIG.raw, sizeof(CONFIG));
    
    if (sizeof(CONFIG) == read_size) {
      if (CONFIG.data.version == CONFIG_VERSION) {
        Serial.println("incorrect config version");
        success = true;
      }
    } else {
      Serial.print("config incorrect size - expected: ");
      Serial.print(sizeof(CONFIG));
      Serial.print(", got: ");
      Serial.println(read_size);
    }
    
    config_file.close();
  } else {
    Serial.println("unable to read config");
    while(true); // watchdog will reboot
  }

  return success;
}

void set_default_config() {
  CONFIG.data.version = CONFIG_VERSION;
  CONFIG.data.last_reading_time = 0;
  CONFIG.data.reading_interval_s = 5 * 60;
  CONFIG.data.cell_configured = 0;
  CONFIG.data.wifi_configured = 0;
  CONFIG.data.endpoint_configured = 0;
}

void print_menu() {
  Serial.println("-------------------------------------");
  Serial.println("[?] Print this menu");
  Serial.println("[r] Set RTC");
  Serial.println("[R] Reset config");
  Serial.println("[E] Exit config");
}

void print_config_info() {
  Serial.println("-------------------------------------");
  Serial.println("Current config:");

  #ifdef WIFI_TRANSMITTER
    if (CONFIG.data.wifi_configured) {
      Serial.print("wifi ssid: ");
      Serial.print(CONFIG.data.wifi_ssid);
      Serial.print(", wifi pass: ");
      Serial.print(CONFIG.data.wifi_pass);
    } else {
      Serial.print("Wifi not configured");
    }

    Serial.println();
  #endif
  
  if (CONFIG.data.cell_configured) {
    Serial.print("hub id: ");
    Serial.print(CONFIG.data.hub_id);
    Serial.print(", cell id: ");
    Serial.print(CONFIG.data.cell_id);
  } else {
    Serial.print("hub/cell id not configured");
  }
  Serial.println();

  if (CONFIG.data.endpoint_configured) {
    Serial.print("endpoint: ");
    Serial.print(CONFIG.data.endpoint_domain);
    Serial.print("/");
    Serial.print(CONFIG.data.endpoint_path);
  } else {
    Serial.print("endpoint not configured");
  }
  Serial.println();
  
  Serial.print("reading_interval (seconds): ");
  Serial.println(CONFIG.data.reading_interval_s);
}

void enter_configuration() {
  print_menu();
    
  while(true) {
    char command = Serial.read();
    while(Serial.available()) { Serial.read(); }
    
    switch (command) {
        case '?': {
          print_menu();
          break;
        }
        case 'r': {
          rtc_set();
          print_menu();
          break;
        }
        case 'R': {
          Serial.println("reseting config");
          set_default_config();
          write_config();
          break;
        }
        case 'p': {
          Serial.println("print config info");
          print_config_info();
          break;
        }
        case 'E': {
          Serial.println("exiting config");
          return;
        }
    }

    watchdog_feed();
    delay(50);
  }
}

void update_last_reading_time(uint32_t timestamp) {
  CONFIG.data.last_reading_time = timestamp;
  write_config();
  Serial.println("updated last reading time");
}
