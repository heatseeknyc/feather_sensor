#include <Arduino.h>
#include "config.h"
#include <SD.h>
#include "watchdog.h"
#include "rtc.h"
#include "transmit.h"

CONFIG_union CONFIG;

void write_config() {
  File config_file;
  
  if (config_file = SD.open("config.bin", FILE_WRITE | O_TRUNC)) {
    config_file.write(CONFIG.raw, sizeof(CONFIG));
    config_file.close();
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
      Serial.print("Version from file: ");
      Serial.print(CONFIG.data.version);
      Serial.print(";  expected version: ");
      Serial.println(CONFIG_VERSION);
      if (CONFIG.data.version == CONFIG_VERSION) {
        Serial.println("config loaded");
        success = true;
      } else {
        Serial.println("incorrect config version");
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
  }

  return success;
}

void set_default_config() {
  CONFIG.data.version = CONFIG_VERSION;
  CONFIG.data.reading_interval_s = 5 * 60;
  CONFIG.data.cell_configured = 0;
  CONFIG.data.wifi_configured = 0;

  strcpy(CONFIG.data.endpoint_domain, "hs-relay-staging.herokuapp.com");
  strcpy(CONFIG.data.endpoint_path, "/temperatures");
  CONFIG.data.endpoint_configured = 1;
}

int read_input_until_newline(char *message, char *buffer) {
  int i = 0;
  bool reached_newline = false;
  
  while (true) {
    Serial.println(message);
    
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        Serial.println("done setting");
        reached_newline = true;
        break;
      } else {
        buffer[i++] = c;
      }
    }
    if (reached_newline) break;
    
    watchdog_feed();
    delay(2000);
  }

  return i;
}

void print_menu() {
  Serial.println("-------------------------------------");
  Serial.println("[?] Print this menu");
  Serial.println("[t] Set RTC");
  Serial.println("[r] Set reading interval");
  #ifdef TRANSMITTER_WIFI
    Serial.println("[w] Setup wifi");
  #endif
  Serial.println("[i] Setup Cell ID");
  Serial.println("[e] Setup API Endpoint");
  Serial.println("[p] Print config");
  Serial.println("[d] Reset config");
  Serial.println("[s] Exit config");
}

void print_config_info() {
  Serial.println("-------------------------------------");
  Serial.println("Current config:");

  #ifdef TRANSMITTER_WIFI
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
        case 't': {
          rtc_set();
          print_menu();
          break;
        }
        case 'r': {
          char buffer[200];
          int length;
          
          length = read_input_until_newline("Enter Reading interval in seconds", buffer);
          buffer[length] = '\0';
          CONFIG.data.reading_interval_s = strtoul(buffer, NULL, 0);

          write_config();

          Serial.println("Reading interval Configured");
          print_config_info();
          print_menu();
          break;
        }
        case 'w': {
          char buffer[200];
          int length;
          
          length = read_input_until_newline("Enter WiFi SSID", buffer);
          buffer[length] = '\0';
          strcpy(CONFIG.data.wifi_ssid, buffer);
          
          length = read_input_until_newline("Enter WiFi password", buffer);
          buffer[length] = '\0';
          strcpy(CONFIG.data.wifi_pass, buffer);

          CONFIG.data.wifi_configured = 1;
          write_config();

          Serial.println("Wifi Configured");
          print_config_info();
          print_menu();
          break;
        }
        case 'i': {
          char buffer[200];
          int length;
          
          length = read_input_until_newline("Enter HUB ID", buffer);
          buffer[length] = '\0';
          strcpy(CONFIG.data.hub_id, buffer);
          
          length = read_input_until_newline("Enter CELL ID", buffer);
          buffer[length] = '\0';
          strcpy(CONFIG.data.cell_id, buffer);

          CONFIG.data.cell_configured = 1;
          write_config();

          Serial.println("Cell ID Configured");
          print_config_info();
          print_menu();
          break;
        }
        case 'e': {
          char buffer[200];
          int length;
          
          length = read_input_until_newline("Enter domain of API endpoint (Example: 'heatseek.org')", buffer);
          buffer[length] = '\0';
          strcpy(CONFIG.data.endpoint_domain, buffer);
          
          length = read_input_until_newline("Enter path of API endpoint (Example: '/readings/create')", buffer);
          buffer[length] = '\0';
          strcpy(CONFIG.data.endpoint_path, buffer);

          CONFIG.data.endpoint_configured = 1;
          write_config();

          Serial.println("API Endpoint configured");
          print_config_info();
          print_menu();
          break;
        }
        case 'd': {
          Serial.println("reseting config");
          set_default_config();
          write_config();
          
          print_config_info();
          print_menu();
          break;
        }
        case 'p': {
          Serial.println("print config info");
          print_config_info();
          break;
        }
        case 's': {
          Serial.println("exiting config");
          return;
        }
    }

    watchdog_feed();
    delay(50);
  }
}

uint32_t get_last_reading_time() {
  File reading_time_file;
  uint8_t data[4];

  if (reading_time_file = SD.open("time.bin", FILE_READ)) {
    reading_time_file.read(data, sizeof(data));
    reading_time_file.close();
  } else {
    Serial.println("unable to read last reading time");
    while(true);
  }

  return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

void update_last_reading_time(uint32_t timestamp) {
  uint8_t data[4];

  data[0] = (timestamp & 0x000000ff);
  data[1] = (timestamp & 0x0000ff00) >> 8;
  data[2] = (timestamp & 0x00ff0000) >> 16;
  data[3] = (timestamp & 0xff000000) >> 24;

  File reading_time_file;

  if (reading_time_file = SD.open("time.bin", FILE_WRITE | O_TRUNC)) {
    reading_time_file.write(data, sizeof(data));
    reading_time_file.close();
  } else {
    Serial.println("unable to update last reading time");
    while(true); // watchdog will reboot
  }

  Serial.println("updated last reading time");
}
