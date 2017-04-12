// CONFIG ======================================

// Select one of the following:

//#define TRANSMITTER_WIFI
#define TRANSMITTER_GSM

// =============================================

//#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "RTClib.h"
#include "DHT.h"
#include <SD.h>

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
//  #include <avr/wdt.h>
//  #include "Adafruit_FONA.h"
//  #include <SoftwareSerial.h>

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
#define PAGE               "/1bl66u81"
#define PORT               80
#define HUB                "00000000test0001"
#define CELL               "Test0000cell0007"

#define READING_DELAY_MS   (2 * 1000)


RTC_PCF8523 rtc;
DHT dht(DHT_DATA, DHT_TYPE);

#ifdef TRANSMITTER_WIFI
  AdafruitHTTP http;
  bool wifiConnected = false;
  volatile bool response_received = false;
#endif

#ifdef TRANSMITTER_GSM
//  SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
//  SoftwareSerial *fonaSerial = &fonaSS;

//  Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
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
  
  if (!SD.begin(SD_CS)) {
    Serial.println("failed to initialize SD card");
    while (true); // watchdog will reboot
  }

  initialize_rtc();

//  dht.begin();
  
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
//  while (true) {
//    sensors_event_t event;
//    bool success = true;
//
//    dht.temperature().getEvent(&event);
//    
//    if (!isnan(event.temperature)) {
//      *temperature_f = dht_util.convertCtoF(event.temperature);
//    } else {
//      success = false;
//    }
//    
//    dht.humidity().getEvent(&event);
//
//    if (!isnan(event.temperature)) {
//      *humidity = event.relative_humidity;
//    } else {
//      success = false;
//    }
//    
//    if (success) {  
//      *heat_index = dht_util.computeHeatIndex(*temperature_f, *humidity);
//  
//      Serial.print("Temperature: ");
//      Serial.print(*temperature_f);
//      Serial.println(" *F");
//      
//      Serial.print("Humidity: ");
//      Serial.print(*humidity);
//      Serial.println("%");
//      
//      Serial.print("Heat index: ");
//      Serial.println(*heat_index);
//
//      return;
//      
//    } else {
//      Serial.println("Error reading temperatures!");
//    }
//  
//    delay(2000);
//
//    // if we continue to fail to read a temperature, the watchdog will
//    // eventually cause a reboot
//  }
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

#ifdef TRANSMITTER_GSM
  void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
//    // POST example based on:
//    // https://github.com/adafruit/Adafruit_FONA/blob/d3d047bf9c47f4f4a2151525d48b5c044daba2e3/Adafruit_FONA.cpp#L1622
//
//    connect_to_fona();
//
//    if (!fona.enableGPRS(true)) {
//      Serial.println(F("Failed to turn on"));
//      while(true);
//    }
//
//    uint16_t statuscode;
//    int16_t length;
//    char *url = "requestb.in/1bl66u81";
//    char *data = "test1=true&test2=false&float=123.3";
//    
//    if (!fona.HTTP_POST_start(url, F("text/plain"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length)) {
//      Serial.println("Failed!");
//      while(true);
//    }
//
//    if (statuscode != 200) {
//      Serial.println("status not 200!");
//    }
//
//    fona.HTTP_POST_end();
  }

//  void connect_to_fona() {
//    fonaSerial->begin(4800);
//    if (! fona.begin(*fonaSerial)) {
//      Serial.println(F("Couldn't find FONA"));
//      while(true); // watchdog will reboot
//    }
//  }
#endif 

#ifdef TRANSMITTER_WIFI
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
#endif

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
  #ifdef TRANSMITTER_WIFI
    iwdg_init(IWDG_PRE_256, 1476); // 9 second watchdog, 40kHz processor
  #endif

  #ifdef TRANSMITTER_GSM
//    wdt_enable(WDTO_8S);
  #endif
}

void watchdog_feed() {
  #ifdef TRANSMITTER_WIFI
    iwdg_feed();
  #endif
  
  #ifdef TRANSMITTER_GSM
//    wdt_reset();
  #endif
}

