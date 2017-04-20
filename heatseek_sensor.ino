// CONFIG ======================================

// Select one of the following:

#define TRANSMITTER_WIFI
//#define TRANSMITTER_GSM

// =============================================

#include <Wire.h>
#include "DHT.h"
#include <SD.h>
#include "config.h"
#include "watchdog.h"
#include "rtc.h"

#ifdef TRANSMITTER_WIFI
  #include <libmaple/iwdg.h>
  #include <adafruit_feather.h>
  #include <adafruit_http.h>

  #define DHT_DATA  PC2
  #define DHT_VCC   PC3         
  #define DHT_GND   PA2
  #define SD_CS     PB4

  #define WLAN_SSID   "0048582303"
  #define WLAN_PASS   "6d3924da63"
#endif

#ifdef TRANSMITTER_GSM
  #include "Adafruit_FONA.h"
  #include <SoftwareSerial.h>
  #include <Adafruit_SleepyDog.h>

  #define DHT_DATA  A2
  #define DHT_VCC   A1         
  #define DHT_GND   A4
  #define SD_CS     10
  
  #define FONA_RX  9
  #define FONA_TX  8
  #define FONA_RST 4
#endif

#define DHT_TYPE           DHT22
#define USER_AGENT_HEADER  "curl/7.45.0"
#define SERVER             "requestb.in"
#define PAGE               "/1haglt01"
#define PORT               80
#define HUB                "00000000test0001"
#define CELL               "Test0000cell0007"

#define READING_INTERVAL_S   (5 * 60)

DHT dht(DHT_DATA, DHT_TYPE);

#ifdef TRANSMITTER_WIFI
  AdafruitHTTP http;
  bool wifiConnected = false;
  volatile bool response_received = false;
#endif

#ifdef TRANSMITTER_GSM
  SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
  SoftwareSerial *fonaSerial = &fonaSS;

  Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
  bool gsmConnected = false;
#endif

void setup() {
  watchdog_init();

  Serial.begin(9600);
  delay(2000);
  Serial.println("initializing heatseek data logger");

  pinMode(DHT_VCC, OUTPUT);
  pinMode(DHT_GND, OUTPUT);
  digitalWrite(DHT_VCC, HIGH);
  digitalWrite(DHT_GND, LOW);

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
  uint32_t last_reading_time = CONFIG.data.last_reading_time;
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
  Serial.println(READING_INTERVAL_S);

//  while (true) {
//    CONFIG.data.version = 1;
//    CONFIG.data.last_reading_time = last_reading_time;
//    strcpy(CONFIG.data.hub_id, "00000000test0001");
//    strcpy(CONFIG.data.cell_id, "Test0000cell0007");
//    write_config();
////
////    read_config();
////
////    Serial.println(CONFIG.data.version);
////    Serial.println(CONFIG.data.hub_id);
//
//    delay(1000);
//    watchdog_feed();
//  }
  
  if (time_since_last_reading < READING_INTERVAL_S) {
    delay(2000);
    watchdog_feed();
    return;
  }
  
  watchdog_feed();
  
  read_temperatures(&temperature_f, &humidity, &heat_index);
  log_to_sd(temperature_f, humidity, heat_index, current_time);
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

#ifdef TRANSMITTER_GSM
  void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    // POST example based on:
    // https://github.com/adafruit/Adafruit_FONA/blob/d3d047bf9c47f4f4a2151525d48b5c044daba2e3/Adafruit_FONA.cpp#L1622

    if (!gsmConnected) connect_to_fona();

    while (!fona_post(temperature_f, humidity, heat_index, current_time)) {
      Serial.println("failed to POST, trying again...");
      delay(500);
    }
  }

  bool fona_post(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    fona.HTTP_POST_end();
              
    uint16_t statuscode;
    int16_t length;
    char *url = "requestb.in/1i65ku31";

    char data[200];

    char temperature_buffer[10];
    char humidity_buffer[10];
    char heat_index_buffer[10];

    dtostrf(temperature_f, 4, 3, temperature_buffer);
    dtostrf(humidity, 4, 3, humidity_buffer);
    dtostrf(heat_index, 4, 3, heat_index_buffer);
    
    sprintf(data, "temp=%s&humidity=%s&heat_index=%s&hub=%s&cell=%s&time=%d", temperature_buffer, humidity_buffer, heat_index_buffer, HUB, CELL, current_time);
    
    while (!fona.HTTP_POST_start(url, F("application/x-www-form-urlencoded"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length)) {
      return false;
    }

    if (statuscode != 200) {
      Serial.print("status not 200: ");
      Serial.println(statuscode);
      return false;
    }

    fona.HTTP_POST_end();
    return true;
  }

  void connect_to_fona() {
    Serial.println('starting fona serial');
    fonaSerial->begin(4800);
    
    Serial.println('starting fona serial 2');
    if (!fona.begin(*fonaSerial)) {
      Serial.println("Couldn't find FONA");
      while(true); // watchdog will reboot
    }

    delay(2000);

    Serial.println('enabling FONA GPRS');

    uint32_t start = millis();
    
    while (!fona.enableGPRS(true)) {
      delay(1000);
      watchdog_feed();

      if (millis() - start > 30000) {
        Serial.println("failed to start FONA GPRS after 30 sec");
        while (true);
      }
    }
    
    Serial.println("Enabled FONA GRPS");

    gsmConnected = true;
  }
#endif 

#ifdef TRANSMITTER_WIFI
  void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    if (!CONFIG.data.cell_configured || !CONFIG.data.wifi_configured || !CONFIG.data.endpoint_configured) {
      Serial.println("cannot send data - not configured");
      return;
    }
  
    while (!wifiConnected) { connect_to_wifi(); }
  
    http.connect(SERVER, PORT); // Will halt if an error occurs
  
    http.addHeader("User-Agent", USER_AGENT_HEADER);
    http.addHeader("Connection", "close");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
    char time_buffer[30];
    char temperature_buffer[30];
    char humidity_buffer[30];
    char heat_index_buffer[30];
  
    sprintf(time_buffer, "%d", current_time);
    sprintf(temperature_buffer, "%.3f", temperature_f);
    sprintf(humidity_buffer, "%.3f", humidity);
    sprintf(heat_index_buffer, "%.3f", heat_index);
  
    const char* post_data[][2] =
    {
      {"hub", HUB},
      {"cell", CELL},
      {"time", time_buffer},
      {"temp", temperature_buffer},
      {"humidity", humidity_buffer},
      {"heat_index", heat_index_buffer}
    };
  
    response_received = false;
  
    http.post(SERVER, PAGE, post_data, 6); // Will halt if an error occurs
  
    while (!response_received);
  
    Serial.println("========================");
  }
  
  void receive_callback(void) {
    http.respParseHeader();
  
    Serial.printf("transmitted - received status: (%d) \n", http.respStatus());
    
    http.stop();
    response_received = true;
  }
  
  void connect_to_wifi() {
    Serial.print("Please wait while connecting to: '" WLAN_SSID "' ... ");
  
    if (Feather.connect(WLAN_SSID, WLAN_PASS)) {
      Serial.println("Connected!");
      wifiConnected = true;
    } else {
      Serial.printf("Failed! %s (%d) \n", Feather.errstr());
    }
    Serial.println();
  
    if (!Feather.connected()) { return; }
  
    // Connected: Print network info
    Feather.printNetwork();
    
    // Tell the HTTP client to auto print error codes and halt on errors
    http.err_actions(true, true);
  
    // Set HTTP client verbose
    http.verbose(true);
  
    // Set the callback handlers
    http.setReceivedCallback(receive_callback);
  }
#endif

void initialize_sd() {
  if (!SD.begin(SD_CS)) {
    Serial.println("failed to initialize SD card");
    while (true); // watchdog will reboot
  }
}
