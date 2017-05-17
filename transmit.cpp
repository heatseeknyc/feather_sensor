#include "transmit.h"
#include "config.h"
#include "watchdog.h"

#ifdef TRANSMITTER_WIFI
  AdafruitHTTP http;
  bool wifiConnected = false;
  volatile bool response_received = false;
#endif

#ifdef TRANSMITTER_GSM
  #include <avr/dtostrf.h>

  HardwareSerial *fonaSerial = &Serial1;

  Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
  bool gsmConnected = false;
#endif

#ifdef TRANSMITTER_GSM
  bool fona_post(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    fona.HTTP_POST_end();
              
    uint16_t statuscode;
    int16_t length;

    char url[200];
    strcpy(url, CONFIG.data.endpoint_domain);
    strcat(url, CONFIG.data.endpoint_path);

    char data[200];

    char temperature_buffer[10];
    char humidity_buffer[10];
    char heat_index_buffer[10];

    dtostrf(temperature_f, 4, 3, temperature_buffer);
    dtostrf(humidity, 4, 3, humidity_buffer);
    dtostrf(heat_index, 4, 3, heat_index_buffer);

    sprintf(data, "temp=%s&humidity=%s&heat_index=%s&hub=%s&cell=%s&time=%d&sp=123", temperature_buffer, humidity_buffer, heat_index_buffer, CONFIG.data.hub_id, CONFIG.data.cell_id, current_time);

    Serial.print("posting to: "); Serial.println(url);
    Serial.print("with data: "); Serial.println(data);

    if (!fona.HTTP_POST_start(url, F("application/x-www-form-urlencoded"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length)) {
      return false;
    }

    Serial.println("reading status");

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

    watchdog_feed();
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

  void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    if (!CONFIG.data.cell_configured || !CONFIG.data.endpoint_configured) {
      Serial.println("cannot send data - not configured");
      return;
    }

    if (!gsmConnected) connect_to_fona();
    watchdog_feed();

    int transmit_attempts = 1;
    
    while (!fona_post(temperature_f, humidity, heat_index, current_time)) {
      Serial.print("failed to POST, trying again... attempt #");
      Serial.println(transmit_attempts);
      
      if (transmit_attempts < 4) {
        transmit_attempts++;
        watchdog_feed();
        delay(500);
      } else {
        while (true);
      }
    }
  }
#endif 

#ifdef TRANSMITTER_WIFI
  void receive_callback(void) {
    http.respParseHeader();
  
    Serial.printf("transmitted - received status: (%d) \n", http.respStatus());
    
    http.stop();
    response_received = true;
  }
  
  void connect_to_wifi() {
    Serial.print("Please wait while connecting to:");
    Serial.print(CONFIG.data.wifi_ssid);
    Serial.println("... ");
  
    if (Feather.connect(CONFIG.data.wifi_ssid, CONFIG.data.wifi_pass)) {
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
  
  void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    if (!CONFIG.data.cell_configured || !CONFIG.data.wifi_configured || !CONFIG.data.endpoint_configured) {
      Serial.println("cannot send data - not configured");
      return;
    }
  
    while (!wifiConnected) { connect_to_wifi(); }
  
    http.connect(CONFIG.data.endpoint_domain, PORT); // Will halt if an error occurs
  
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
      {"hub", CONFIG.data.hub_id},
      {"cell", CONFIG.data.cell_id},
      {"time", time_buffer},
      {"temp", temperature_buffer},
      {"humidity", humidity_buffer},
      {"sp", "123"},
      {"heat_index", heat_index_buffer}
    };
  
    response_received = false;
  
    http.post(CONFIG.data.endpoint_domain, CONFIG.data.endpoint_path, post_data, 7); // Will halt if an error occurs
  
    while (!response_received);
  
    Serial.println("========================");
  }
#endif
