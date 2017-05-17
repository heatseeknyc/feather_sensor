#include <Wire.h>
#include <DHT.h>
#include <SD.h>
#include "transmit.h"
#include "config.h"
#include "watchdog.h"
#include "rtc.h"

DHT dht(DHT_DATA, DHT22);

void setup() {
  watchdog_init();

  Serial.begin(9600);
  delay(2000);
  Serial.println("initializing heatseek data logger");

  initialize_sd();
  rtc_initialize();

  dht.begin();

  if (!read_config()) set_default_config();

  watchdog_feed();
}

void loop() {
  float temperature_f;
  float humidity;
  float heat_index;

  uint32_t current_time = rtc.now().unixtime();
  uint32_t last_reading_time = get_last_reading_time();
  uint32_t time_since_last_reading = current_time - last_reading_time;

  char command = Serial.read();
  if (command == 'C') {
    enter_configuration();
  }
      
  Serial.print("time since last reading: ");
  Serial.print(time_since_last_reading);
  Serial.print(", current_time: ");
  Serial.print(current_time);
  Serial.print(", last_reading_time: ");
  Serial.print(last_reading_time);
  Serial.print(", reading_interval: ");
  Serial.println(CONFIG.data.reading_interval_s);
  
  if (time_since_last_reading < CONFIG.data.reading_interval_s) {
    delay(2000);
    watchdog_feed();
    return;
  }
  
  watchdog_feed();
  
  read_temperatures(&temperature_f, &humidity, &heat_index);
  log_to_sd(temperature_f, humidity, heat_index, current_time);

  watchdog_feed();
  
  update_last_reading_time(current_time);
  watchdog_feed();
  
  transmit(temperature_f, humidity, heat_index, current_time);

  watchdog_feed();

  delay(2000);
}

void read_temperatures(float *temperature_f, float *humidity, float *heat_index) {
  while (true) {
    bool success = true;

    *temperature_f = dht.readTemperature(true);
    *humidity = dht.readHumidity();
    
    if (!isnan(*temperature_f) && !isnan(*humidity)) {  
      *heat_index = dht.computeHeatIndex(*temperature_f, *humidity);
  
      Serial.print("Temperature: ");
      Serial.print(*temperature_f);
      Serial.println(" *F");
      
      Serial.print("Humidity: ");
      Serial.print(*humidity);
      Serial.println("%");
      
      Serial.print("Heat index: ");
      Serial.println(*heat_index);

      return;
      
    } else {
      Serial.println("Error reading temperatures!");
    }
  
    delay(2000);

    // if we continue to fail to read a temperature, the watchdog will
    // eventually cause a reboot
  }
}

void log_to_sd(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
  Serial.println("writing to SD card...");
  
  File data_file;
  
  if (data_file = SD.open("data.csv", FILE_WRITE)) {
    data_file.print(current_time); data_file.print(",");
    data_file.print(temperature_f); data_file.print(",");
    data_file.print(humidity); data_file.print(",");
    data_file.print(heat_index); data_file.println();
    Serial.println("wrote to SD");
    data_file.close();
  } else {
    Serial.println("unable to open data.csv");
    while(true); // watchdog will reboot
  }
}

void initialize_sd() {
  if (!SD.begin(SD_CS)) {
    Serial.println("failed to initialize SD card");
    while (true); // watchdog will reboot
  }
}
