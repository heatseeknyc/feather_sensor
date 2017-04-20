#include "transmit.h"
#include "config.h"

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
