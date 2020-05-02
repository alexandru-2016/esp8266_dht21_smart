#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#include "MedianFilterLib.h"

#include "secrets.h"

#define HOSTNAME "esp8266-bedroom-dht21"

#define DHTPIN 2 // D1 mini pin D4
#define DHTTYPE DHT21   // AM2301

DHT dht(DHTPIN, DHTTYPE);

ESP8266WebServer server(80);

Ticker sensor_read_tick;

bool enable_sensor_read = false;

float humidity;
float temperature;
int sensor_err = 1;

MedianFilter<float> humidity_median_filter(5);
MedianFilter<float> temperature_median_filter(5);

void setup() {  
  Serial.begin(115200);
    
  Serial.println();
  Serial.print("Firmware version: ");
  Serial.println(VERSION);

  Serial.println("DHT21 sensor");
  dht.begin();

  setup_wpa2();

  bool wifiOk = check_wifi(30);
  if (!wifiOk) {
    ESP.restart();
  }

  setup_server();

  sensor_read_tick.attach(10, tick_sensor_read);

  read_sensor();
}

void setup_wpa2() {  
  char* ssid = WPA2_SSID;
  char* password = WPA2_PASS;

  Serial.println();
  Serial.print("Connecting to WPA2: ");
  Serial.println(ssid);

  WiFi.hostname(HOSTNAME);
  WiFi.begin(ssid, password);
}

bool check_wifi(int secondsTimeout) {
  int retryCount = 0;
  
  while (WiFi.status() != WL_CONNECTED && retryCount < secondsTimeout) {
      delay(1000);
      Serial.print(".");
      retryCount++;
  }
  Serial.println("");

  bool wifiOk = WiFi.status() == WL_CONNECTED;

  if (wifiOk) {
    print_wifi_connected();
  } else {
    Serial.println("WiFi cannot connect");
  }

  return wifiOk;
}

void print_wifi_connected() {
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_server() {  
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot() {
  if (sensor_err) {
    server.send(503, "text/plain", "Cannot read sensor");
  }

  char data[256];

  snprintf(data, 256, "{ \"temperature\": %.2f, \"humidity\": %.2f }",
          temperature, humidity);
          
  Serial.println("HTTP response sent");
  
  server.send(200, "text/plain", data);
}

void tick_sensor_read() {
  // it is not recommended to do I/O on a ticker, just set a flag
  enable_sensor_read = true;
}

void loop() {
  if (enable_sensor_read) {
    enable_sensor_read = false;
    read_sensor();
  }
  
  server.handleClient();
}

float read_data(float (*reader)(), String readerName) {
  float tmp_value = NULL;

  // do multiple read until two consecutive values are returned, to try to stabilise the reading
  for (int i = 0; i < 5; i++) {
    delay(50);

    float step_value = reader();

    if (isnan(step_value)) {
      Serial.println("Failed to read value for: " + readerName);
      continue;     
    }

    if (tmp_value == step_value) {
      return step_value;
    }

    tmp_value = step_value;
  }
  
  return tmp_value;
}

float read_humidity() {  
  float tmp_humidity = read_data(do_read_humidty, "humidity");
  tmp_humidity = humidity_median_filter.AddValue(tmp_humidity);

  return tmp_humidity;
}

float do_read_humidty() {
  return dht.readHumidity();
}

float read_temperature() {
  float tmp_temperature = read_data(do_read_temperature, "temperature");
  tmp_temperature = temperature_median_filter.AddValue(tmp_temperature);

  return tmp_temperature;
}

float do_read_temperature() {
  return dht.readTemperature();
}

void read_sensor() {
  sensor_err = 0;
  
  humidity = read_humidity();
  temperature = read_temperature();
  
  if (isnan(temperature) || isnan(humidity)) 
  {
    Serial.println("Failed to read from DHT");
    sensor_err = -1;
  }
  else {
    //Print temp and humidity values to serial monitor
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print(" %, Temp: ");
    Serial.print(temperature);
    Serial.println(" Celsius");
    sensor_err = 0;
  }
}
