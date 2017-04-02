#define TRANSMITTER_WIFI
// #define TRANSMITTER_2G
// #define TRANSMITTER_LORA


#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "RTClib.h"
#include "DHT_U.h"
#include <SD.h>

#ifdef TRANSMITTER_WIFI
  #include <libmaple/iwdg.h>
  #include <adafruit_feather.h>
  #include <adafruit_http.h>

  #define DHT_DATA  PC2
  #define DHT_VCC   PC3         
  #define DHT_GND   PA2
  #define DHT_TYPE  DHT22
  #define SD_CS     PB4

  #define WLAN_SSID   "0048582303"
  #define WLAN_PASS   "6d3924da63"
#endif

#define USER_AGENT_HEADER  "curl/7.45.0"
#define SERVER             "requestb.in"
#define PAGE               "/1b6wrxb1"
#define PORT               80
#define HUB                "00000000test0001"
#define CELL               "Test0000cell0007"

#define READING_DELAY_MS   (2 * 1000)


RTC_PCF8523 rtc;
DHT_Unified dht(DHT_DATA, DHT_TYPE);
DHT dht_util(DHT_DATA, DHT_TYPE);

#ifdef TRANSMITTER_WIFI
  AdafruitHTTP http;
  bool wifiConnected = false;
  volatile bool response_received = false;
#endif


void setup() {
  watchdog_init();

  Serial.begin(9600); 
  while (!Serial) { delay(1); }
  Serial.println("initializing heatseek data logger");

  pinMode(DHT_VCC, OUTPUT);
  pinMode(DHT_GND, OUTPUT);
  digitalWrite(DHT_VCC, HIGH);
  digitalWrite(DHT_GND, LOW);
  
  if (!SD.begin(SD_CS)) {
    Serial.println("failed to initialize SD card");
    while (true); // watchdog will reboot
  }

  initialize_rtc();

  dht.begin();
  
  watchdog_feed();
}

void loop() {
  float temperature_f;
  float humidity;
  float heat_index;

  uint32_t current_time = rtc.now().unixtime();
  read_temperatures(&temperature_f, &humidity, &heat_index);
  log_to_sd(temperature_f, humidity, heat_index, current_time);
  watchdog_feed();
  
  transmit(temperature_f, humidity, heat_index, current_time);

  watchdog_feed();
  delay(READING_DELAY_MS);
}

void read_temperatures(float *temperature_f, float *humidity, float *heat_index) {
  while (true) {
    sensors_event_t event;
    bool success = true;

    dht.temperature().getEvent(&event);
    
    if (!isnan(event.temperature)) {
      *temperature_f = dht_util.convertCtoF(event.temperature);
    } else {
      success = false;
    }
    
    dht.humidity().getEvent(&event);

    if (!isnan(event.temperature)) {
      *humidity = event.relative_humidity;
    } else {
      success = false;
    }
    
    if (success) {  
      *heat_index = dht_util.computeHeatIndex(*temperature_f, *humidity);
  
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

void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
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

void initialize_rtc() {
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (true); // watchdog will reboot
  }
  
  if (!rtc.initialized()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    // TODO - this could make a web request to set and continually update the RTC

    char timestamp[50];
    int i = 0;
    bool timestamp_input = false;

    Serial.println("RTC needs to be set, enter the current unix timestamp");

    while (true) {
      if (Serial.available()) {
        char c = Serial.read();
        Serial.print("read: ");
        Serial.println(c);
        if (c == '\n') {
          Serial.println("done setting");
          break;
        } else {
          timestamp[i++] = c;
        }
      }
    }
    
    rtc.adjust(DateTime(atoi(timestamp)));
  }
}

void watchdog_init() {
  iwdg_init(IWDG_PRE_256, 1476); // 9 second watchdog, 40kHz processor
}

void watchdog_feed() {
  iwdg_feed();
}

