#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_VERSION     3

typedef struct {
  uint16_t version;
  uint32_t last_reading_time;
  uint32_t reading_interval_s;

  uint8_t cell_configured;
  char hub_id[50];
  char cell_id[50];

  uint8_t wifi_configured;
  char wifi_ssid[50];
  char wifi_pass[200];

  uint8_t endpoint_configured;
  char endpoint_domain[100];
  char endpoint_path[100];
} CONFIG_struct;

typedef union {
  CONFIG_struct data;
  uint8_t raw[sizeof(CONFIG_struct)];
} CONFIG_union;

extern CONFIG_union CONFIG;

void write_config();
bool read_config();
void set_default_config();
void enter_configuration();
void update_last_reading_time(uint32_t timestamp);

#endif
