#include <Arduino.h>
#include <DHT.h>
#include "config.h"
#include <SD.h>
#include "watchdog.h"
#include "rtc.h"
#include "transmit.h"

#ifdef HEATSEEK_FEATHER_WIFI_WICED
char const* get_encryption_str(int32_t enc_type);
#endif

CONFIG_union CONFIG;
static DHT dht(DHT_DATA, DHT22);

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
  CONFIG.data.temperature_offset_f = 0.0;
  
  strcpy(CONFIG.data.hub_id, "featherhub");
  
  strcpy(CONFIG.data.endpoint_domain, "relay.heatseek.org");
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
//        Serial.println("done setting");
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
  Serial.println("[q] Clear reading transmission queue");
  Serial.println("[v] Calibrate temperature sensor");
  #ifdef TRANSMITTER_WIFI
    Serial.println("[w] Setup wifi");
  #endif
  // #ifdef HEATSEEK_FEATHER_WIFI_WICED
  //   Serial.println("[a] List nearby access points");
  // #endif
  #ifdef TRANSMITTER_WIFI
    Serial.println("[a] List nearby access points");
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
    Serial.print("cell id not configured");
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
        case 'q': {
          clear_queued_transmissions();
          print_menu();
          break;
        }
        case 'r': {
          char buffer[200];
          int length;
          
          length = read_input_until_newline("Enter Reading interval in seconds", buffer);
          buffer[length] = '\0';
          CONFIG.data.reading_interval_s = strtol(buffer, NULL, 0);

          write_config();

          update_last_reading_time(0); // take initial reading after configuration
          clear_queued_transmissions(); // clear transmission queue

          Serial.println("Reading interval Configured");
          print_config_info();
          print_menu();
          break;
        }
#ifdef TRANSMITTER_WIFI
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
          force_wifi_reconnect();

          Serial.println("Wifi Configured");
          print_config_info();
          print_menu();
          break;
        }
#endif
#ifdef HEATSEEK_FEATHER_WIFI_WICED
        case 'a': {
          wl_ap_info_t ap_list[20];
          int networkCount = 0;
          networkCount = Feather.scanNetworks(ap_list, 20);
          
          Serial.println("=========");
          Serial.print("Found "); Serial.print(networkCount); Serial.println(" Networks");

          for (int i = 0; i < networkCount; i++) {
            Serial.println("=========");
            wl_ap_info_t ap = ap_list[i];
            Serial.print("SSID: "); Serial.println(ap.ssid);
            Serial.print("RSSI: "); Serial.println(ap.rssi);
            Serial.print("max data rate: "); Serial.println(ap.max_data_rate);
            Serial.print("network type: "); Serial.println(ap.network_type);
            Serial.print("security: "); Serial.print(ap.security); Serial.print(" - "); Serial.println(get_encryption_str(ap.security));
            Serial.print("channel: "); Serial.println(ap.channel);
            Serial.print("band_2_4ghz: "); Serial.println(ap.band_2_4ghz);
          }
          
          break;
        }
#endif
#ifdef HEATSEEK_FEATHER_WIFI_M0
        case 'a': {
          // Derived from ScanNetworks sample of WiFi101 library
          // TODO: Assuming that WiFi is available here when it totally could not be
          int networkCount = 0;
          networkCount = WiFi.scanNetworks();

          Serial.println("=========");
          Serial.print("Found "); Serial.print(networkCount); Serial.println(" Networks");
          
          for (int i = 0; i < networkCount; i++) {
            Serial.println("=========");
            Serial.print("SSID: "); Serial.println(WiFi.SSID(i));
            Serial.print("RSSI: "); Serial.println(WiFi.RSSI(i));
            Serial.print("security: ");
            switch (WiFi.encryptionType(i)) {
              case ENC_TYPE_WEP:
                Serial.println("WEP");
              case ENC_TYPE_TKIP:
                Serial.println("WPA");
              case ENC_TYPE_CCMP:
                Serial.println("WPA2");
              case ENC_TYPE_NONE:
                Serial.println("None");
              case ENC_TYPE_AUTO:
                Serial.println("Auto");
            }
          }
          break;
        }
#endif
        case 'i': {
          char buffer[200];
          int length;

//          hub is now always set to 'featherhub'
//          length = read_input_until_newline("Enter HUB ID", buffer);
//          buffer[length] = '\0';
//          strcpy(CONFIG.data.hub_id, buffer);
          
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
        case 'v': {
          char buffer[200];
          int length;
          float current_temp;
          int readings_taken = 0;
          float temperature_f;
          float average_temperature_f = 0.0;
          
          length = read_input_until_newline("Enter current temperature, in fahrenheit, with one decimal place.  For example: '41.0'.  PLEASE ENSURE THAT SENSOR IS ON FOR APPROX. 5 MINUTES BEFORE CALIBRATING!", buffer);
          buffer[length] = '\0';
          current_temp = strtof(buffer, NULL);

          while (readings_taken < 5) {
            watchdog_feed();
            Serial.println("Calibrating, please wait...");
            temperature_f = dht.readTemperature(true);
            
            if (!isnan(temperature_f)) {
              if (average_temperature_f == 0.0) {
                average_temperature_f = temperature_f;
              } else {
                average_temperature_f = (average_temperature_f + temperature_f) / 2;
              }
            } else {
              Serial.println("Failed to read temperature sensor; reboot device and try again.");
              while(true);
            }
            
            watchdog_feed();
            readings_taken++;
            delay(2000);
          }

          watchdog_feed();
          
          float temperature_offset = current_temp - average_temperature_f;
          CONFIG.data.temperature_offset_f = temperature_offset;
          write_config();
          
          Serial.print("Temperature offset set to: ");
          Serial.println(temperature_offset);
          
          print_config_info();
          print_menu();
          break;
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
    return 0;
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

#ifdef HEATSEEK_FEATHER_WIFI_WICED
char const* get_encryption_str(int32_t enc_type)
{
  // read the encryption type and print out the name:
  switch (enc_type)
  {
    case ENC_TYPE_AUTO: return "ENC_TYPE_AUTO";
    case ENC_TYPE_OPEN: return "ENC_TYPE_OPEN";
    case ENC_TYPE_WEP: return "ENC_TYPE_WEP";
    case ENC_TYPE_WEP_SHARED: return "ENC_TYPE_WEP_SHARED";
    case ENC_TYPE_WPA_TKIP: return "ENC_TYPE_WPA_TKIP";
    case ENC_TYPE_WPA_AES: return "ENC_TYPE_WPA_AES";
    case ENC_TYPE_WPA_MIXED: return "ENC_TYPE_WPA_MIXED";
    case ENC_TYPE_WPA2_AES: return "ENC_TYPE_WPA2_AES";
    case ENC_TYPE_WPA2_TKIP: return "ENC_TYPE_WPA2_TKIP";
    case ENC_TYPE_WPA2_MIXED: return "ENC_TYPE_WPA2_MIXED";
    case ENC_TYPE_WPA_TKIP_ENT: return "ENC_TYPE_WPA_TKIP_ENT";
    case ENC_TYPE_WPA_AES_ENT: return "ENC_TYPE_WPA_AES_ENT";
    case ENC_TYPE_WPA_MIXED_ENT: return "ENC_TYPE_WPA_MIXED_ENT";
    case ENC_TYPE_WPA2_TKIP_ENT: return "ENC_TYPE_WPA2_TKIP_ENT";
    case ENC_TYPE_WPA2_AES_ENT: return "ENC_TYPE_WPA2_AES_ENT";
    case ENC_TYPE_WPA2_MIXED_ENT: return "ENC_TYPE_WPA2_MIXED_ENT";
    case ENC_TYPE_WPS_OPEN: return "ENC_TYPE_WPS_OPEN";
    case ENC_TYPE_WPS_SECURE: return "ENC_TYPE_WPS_SECURE";
    case ENC_TYPE_IBSS_OPEN: return "ENC_TYPE_IBSS_OPEN";
    default: return "UNKNOWN";
  }
}
#endif
