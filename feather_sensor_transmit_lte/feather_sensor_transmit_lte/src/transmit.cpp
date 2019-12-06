#include "transmit.h"
#include "config.h"
#include "watchdog.h"
#include <SD.h>

#ifdef HEATSEEK_BORON_LTE
	TCPClient tcpClient;
	bool lteConnected = false;
#endif

#ifdef HEATSEEK_FEATHER_WIFI_WICED
  AdafruitHTTP http;
  bool wifiConnected = false;
  volatile bool response_received = false;
  volatile bool transmit_success = false;
#endif

#ifdef HEATSEEK_FEATHER_WIFI_M0
  WiFiClient wifiClient;
  bool wifiConnected = false;
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

    sprintf(data, "temp=%s&humidity=%s&heat_index=%s&hub=%s&cell=%s&time=%d&sp=%d&cell_version=%s", temperature_buffer, humidity_buffer, heat_index_buffer, CONFIG.data.hub_id, CONFIG.data.cell_id, current_time, CONFIG.data.reading_interval_s, CODE_VERSION);

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

      if (millis() - start > 60000) {
        Serial.println("failed to start FONA GPRS after 60 sec");
        while (true);
      }
    }
    
    Serial.println("Enabled FONA GRPS");

    gsmConnected = true;
  }

  bool _transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    if (!CONFIG.data.cell_configured || !CONFIG.data.endpoint_configured) {
      Serial.println("cannot send data - not configured");
      return false;
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

    return true;
  }
#endif 

#ifdef HEATSEEK_FEATHER_WIFI_WICED
  void force_wifi_reconnect(void) {
    wifiConnected = false;
  }

  void receive_callback(void) {
    http.respParseHeader();
    int status_received = http.respStatus();
    
    Serial.printf("transmitted - received status: (%d) \n", status_received);
    
    http.stop();
    response_received = true;
    transmit_success = (status_received == 200);
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
  
  bool _transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    if (!CONFIG.data.cell_configured || !CONFIG.data.wifi_configured || !CONFIG.data.endpoint_configured) {
      Serial.println("cannot send data - not configured");
      return false;
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
    char reading_interval_buffer[30];
  
    sprintf(time_buffer, "%d", current_time);
    sprintf(reading_interval_buffer, "%d", CONFIG.data.reading_interval_s);
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
      {"sp", reading_interval_buffer},
      {"heat_index", heat_index_buffer},
      {"cell_version", CODE_VERSION},
    };
    int param_count = 8;
  
    response_received = false;
    transmit_success = false;
  
    http.post(CONFIG.data.endpoint_domain, CONFIG.data.endpoint_path, post_data, param_count); // Will halt if an error occurs
  
    while (!response_received || !transmit_success); // Hang if transmit doesn't complete or fails

    return true;
  }
#endif

#ifdef HEATSEEK_FEATHER_WIFI_M0
  void force_wifi_reconnect(void) {
    if (wifiConnected) {
      wifiConnected = false;
      WiFi.end();
    }
  }
  
  void connect_to_wifi() {
    WiFi.setPins(8, 7, 4, 2);
    
    Serial.print("Please wait while connecting to:");
    Serial.print(CONFIG.data.wifi_ssid);
    Serial.println("... ");

    int status = WL_IDLE_STATUS;

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(CONFIG.data.wifi_ssid, CONFIG.data.wifi_pass);
    
    while (status != WL_CONNECTED) {
      delay(1000);
      Serial.println("Establishing connection...");
    }

    wifiConnected = true;
    Serial.println("Connected to WiFi");

    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
  
    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");

    watchdog_feed();
  }
  
  bool _transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
    if (!CONFIG.data.cell_configured || !CONFIG.data.wifi_configured || !CONFIG.data.endpoint_configured) {
      Serial.println("cannot send data - not configured");
      return false;
    }
  
    if (!wifiConnected) { connect_to_wifi(); }

    HttpClient client = HttpClient(wifiClient, CONFIG.data.endpoint_domain, 80);

    String contentType = "application/x-www-form-urlencoded";
    String data = "temp=" + String(temperature_f, 3) + "&humidity=" + String(humidity, 3) + "&heat_index=" + String(heat_index, 3) + "&hub=" + CONFIG.data.hub_id + "&cell=" + CONFIG.data.cell_id + "&time=" + current_time + "&sp=" + CONFIG.data.reading_interval_s + "&cell_version=" + CODE_VERSION;   

    Serial.print("Posting data: ");
    Serial.println(data);

    client.post(CONFIG.data.endpoint_path, contentType, data);

    int statusCode = client.responseStatusCode();
    String response = client.responseBody();
  
    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
  
    return statusCode == 200;
  }
#endif

#ifdef HEATSEEK_BORON_LTE
  // Function to reconnect to LTE?
  void force_lte_reconnect(void);

	// Function to connect to Particle Cloud
	void connect_to_lte(){
    // Configure the connection here.
    Serial.println("Establishing connection...");
    // Connect to LTE network here.
    Serial.println("Connected to LTE");
    watchdog_feed();
  }
  
  // Function to transmit
  bool _transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time){
	  // If cell / API endpoint not configured, indicate error
    if (!CONFIG.data.cell_configured || !CONFIG.data.endpoint_configured) {
      Serial.println("cannot send data - not configured");
      return false;
    }

    // If not connected, connect
    if(!lteConnected){
      connect_to_lte();
    }

    // Set up TCPClient
    // Do some error checking
    tcpClient.connect(CONFIG.data.endpoint_domain, 80);
    String contentType = "application/x-www-form-urlencoded";
    String data = "temp=" + String(temperature_f, 3) + "&humidity=" + String(humidity, 3) + "&heat_index=" + String(heat_index, 3) + "&hub=" + CONFIG.data.hub_id + "&cell=" + CONFIG.data.cell_id + "&time=" + current_time + "&sp=" + CONFIG.data.reading_interval_s + "&cell_version=" + CODE_VERSION;  
    Serial.print("Posting data: ");
    Serial.println(data);
    tcpClient.println(data);
    int statusCode = tcpClient.status();
    int numCharacters = tcpClient.available();
    String response = "";
    for (int i = 0; i < numCharacters; i++)
    {
      response += tcpClient.read();
    }
    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
    return statusCode == 200;
    // Load data
    // Post data via TCPClient
    // Done.
}
#endif

typedef struct {
  float temperature_f;
  float humidity;
  float heat_index;
} temp_data_struct;

typedef union {
  temp_data_struct data;
  uint8_t raw[sizeof(temp_data_struct)];
} temp_data;

// Create a file on the SD card representing a temperature reading
// The file name is the unix timestamp, however, due to FAT naming conventions,
// we have to place a period in it.   e.g.   1500985299   ->  1500985.299
// The content of the file is the binary reading data struct.
void queue_transmission(char *filename, temp_data temp, uint32_t current_time) {
  char file_path[50];
  char timestamp[50];
  char timestamp_first_half[50];
  char timestamp_last_half[50];

  // FAT only allows 8 character names, so we make the last 3 digits of the 
  // timestamp the file "extension"
  sprintf(timestamp, "%d", current_time);
  strncpy(timestamp_first_half, timestamp, 7);
  timestamp_first_half[7] = '\0';
  strncpy(timestamp_last_half, timestamp+7, 3);
  timestamp_last_half[3] = '\0';

  sprintf(filename, "%s.%s", timestamp_first_half, timestamp_last_half);
  sprintf(file_path, "pending/%s", filename);

  File temperature_file;
  SD.mkdir("pending");
  if (temperature_file = SD.open(file_path, FILE_WRITE | O_TRUNC)) {
    temperature_file.write(temp.raw, sizeof(temp));
    temperature_file.close();
  } else {
    Serial.println("unable to write temperature");
    while(true); // watchdog will reboot
  }
}

// Take a filename for a reading, read the temperature data and
// transmit it.  Then delete the file when the transfer is successful.
void transmit_queued_temp(char *filename) {
  watchdog_feed();
  
  File temperature_file;
  temp_data temperature;
  char read_time_buffer[100];
  char file_path[100];
  sprintf(file_path, "pending/%s", filename);
  
  Serial.print("transfering: ");
  Serial.println(filename);

  strncpy(read_time_buffer, filename, 7);
  strncpy(read_time_buffer+7, filename+8, 3);
  read_time_buffer[10] = '\0';

  uint32_t read_time = strtoul(read_time_buffer, NULL, 0);

  if (temperature_file = SD.open(file_path, FILE_READ)) {
    int read_size = temperature_file.read(temperature.raw, sizeof(temperature));
    
    if (sizeof(temperature) == read_size) {
      bool transmit_success = _transmit(temperature.data.temperature_f, temperature.data.humidity, temperature.data.heat_index, read_time);
      
      temperature_file.close();
      delay(100);
        
      if (transmit_success) {
        Serial.println("transferred.");

        if (SD.remove(file_path)) {
          Serial.println("removed.");
        } else {
          Serial.println("failed to remove file");
        }
      } else {
        Serial.println("failed to transfer");
      }
    } else {
      Serial.print("file incorrect size - expected: ");
      Serial.print(sizeof(temperature));
      Serial.print(", got: ");
      Serial.println(read_size);
    }
  } else {
    Serial.print("failed to open: ");
    Serial.println(filename);
  }
    
  watchdog_feed();
}

void transmit_queued_temps() {
  char filename[100];
  File pending_dir = SD.open("pending");
  int temps_transfered_count = 0;

  while (temps_transfered_count < TRANSMITS_PER_LOOP) {
    File entry = pending_dir.openNextFile();
    if (!entry) { break; } // No more files
    
    strcpy(filename, entry.name());
    entry.close();
    
    transmit_queued_temp(filename);
    temps_transfered_count += 1;
  }

  pending_dir.close();
}

void clear_queued_transmissions() {
  File pending_dir = SD.open("pending");

  Serial.println("==== Removing queued temperature files");
  while (true) {
    watchdog_feed();
    
    File entry = pending_dir.openNextFile();
    if (!entry) { break; } // No more files

    char filename[100];
    strcpy(filename, entry.name());
    entry.close();

    char file_path[100];
    sprintf(file_path, "pending/%s", filename);
  
    if (SD.remove(file_path)) {
      Serial.println("removed queued temperature file.");
    } else {
      Serial.println("failed to remove queued temperature file.");
    }
  }
  
  Serial.println("====");

  pending_dir.close();
}

void transmit(float temperature_f, float humidity, float heat_index, uint32_t current_time) {
  watchdog_feed();
  
  temp_data temp;
  temp.data.temperature_f = temperature_f;
  temp.data.humidity = humidity;
  temp.data.heat_index = heat_index;

  char filename[100];
  queue_transmission(filename, temp, current_time);
  watchdog_feed();
  delay(1000);
    
  Serial.print("created file: ");
  Serial.println(filename);

  transmit_queued_temp(filename);
  transmit_queued_temps();
}
