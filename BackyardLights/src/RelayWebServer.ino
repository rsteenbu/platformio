#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_VEML7700.h>
#include <OneWire.h>

#include <Adafruit_AM2315.h>
Adafruit_AM2315 am2315;

#include <my_relay.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#define MYTZ TZ_America_Los_Angeles

ESP8266WebServer server(80);

#define JSON_SIZE 300
#define SYSTEM_APPNAME "system"
#define LIGHT_APPNAME "lightsensor"
#define THERMO_APPNAME "thermometer"
#define LIGHTSWITCH_APPNAME "lightswitch"

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, SYSTEM_APPNAME, LOG_LOCAL0);

int debug = 0;

// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;

char msg[40];
StaticJsonDocument<200> doc;
Relay * lvLights = new Relay(TX_PIN);

Adafruit_VEML7700 veml = Adafruit_VEML7700();

bool vemlSensorFound = false;
bool thermoSensorFound = false;
int16_t lightLevel = 0;
float temperature, humidity = -1;

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // Setup the Relay
  lvLights->setup("backyard_lights");
  lvLights->setBackwards();

  // set I2C pins (SDA, SDL)
  Wire.begin(GPIO2_PIN, GPIO0_PIN);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to network...");
    delay(500);
  }

  sprintf(msg, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  // Start the HTTP server
  server.on("/debug", handleDebug);
  server.on("/light", handleLight);
  server.on("/status", handleStatus);
  server.on("/sensors", handleSensors);
  server.begin();
  syslog.appName(LIGHT_APPNAME);
  int attempts = 0; 
  while (attempts < 10) {
    if (veml.begin()) {
      vemlSensorFound = true;
      syslog.log(LOG_INFO, "VEML Sensor found");
      break;
    }
    syslog.logf(LOG_INFO, "VEML Sensor not found, try %d", attempts);
    attempts++; 
    delay(1000);
  } 

  if (!vemlSensorFound) {
    syslog.logf(LOG_INFO, "VEML Sensor not found, giving up");
  } else {
    switch (veml.getGain()) {
      case VEML7700_GAIN_1: syslog.logf(LOG_INFO, "Gain: %s", "1"); break;
      case VEML7700_GAIN_2: syslog.logf(LOG_INFO, "Gain: %s", "2"); break;
      case VEML7700_GAIN_1_4: syslog.logf(LOG_INFO, "Gain: %s", "1/4"); break;
      case VEML7700_GAIN_1_8: syslog.logf(LOG_INFO, "Gain: %s", "1/8"); break;
    }

    Serial.print(F("Integration Time (ms): "));
    switch (veml.getIntegrationTime()) {
      case VEML7700_IT_25MS: syslog.logf(LOG_INFO, "Integration Time (ms): %d", 25); break;
      case VEML7700_IT_50MS: syslog.logf(LOG_INFO, "Integration Time (ms): %d", 50); break;
      case VEML7700_IT_100MS: syslog.logf(LOG_INFO, "Integration Time (ms): %d", 100); break;
      case VEML7700_IT_200MS: syslog.logf(LOG_INFO, "Integration Time (ms): %d", 200); break;
      case VEML7700_IT_400MS: syslog.logf(LOG_INFO, "Integration Time (ms): %d", 400); break;
      case VEML7700_IT_800MS: syslog.logf(LOG_INFO, "Integration Time (ms): %d", 800); break;
    }
    veml.setLowThreshold(10000);
    veml.setHighThreshold(20000);
    veml.interruptEnable(false);
  }

  syslog.appName(THERMO_APPNAME);
  attempts = 0;
  while (attempts < 10) {
    if (am2315.begin()) {
      thermoSensorFound = true;
      syslog.log(LOG_INFO, "AMS23315 Sensor found!");
      break;
    }
    syslog.logf(LOG_INFO, "AMS23315 Sensor not found, try %d", attempts);
    attempts++; 
    delay(1000);
  } 

  if (!thermoSensorFound) {
    syslog.logf(LOG_INFO, "AMS23315 Sensor not found, giving up");
  } 
  syslog.appName(SYSTEM_APPNAME);
}

void handleDebug() {
  if (server.arg("level") == "0") {
    syslog.log(LOG_INFO, "Debug level 0");
    debug = 0;
    server.send(200, "text/plain");
  } else if (server.arg("level") == "1") {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
    server.send(200, "text/plain");
  } if (server.arg("level") == "2") {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
    server.send(200, "text/plain");
  } else if (server.arg("level") == "status") {
    char msg[40];
    sprintf(msg, "Debug level: %d", debug);
    server.send(200, "text/plain", msg);
  } else {
    server.send(404, "text/plain", "ERROR: unknown debug command");
  }
}

void handleLight() {
  if (server.arg("state") == "on") {
    lvLights->switchOn();
    syslog.logf(LOG_INFO, "Turned %s on", lvLights->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    lvLights->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off", lvLights->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", lvLights->on ? "1" : "0");
  } else {
    server.send(404, "text/plain", "ERROR: unknown light command");
  }
}

void handleSensors() {
  if (server.arg("sensor") == "humidity") {
    char msg[10];
    sprintf(msg, "%0.2f", humidity);
    server.send(200, "text/plain", msg);
  }
  if (server.arg("sensor") == "light") {
    char msg[10];
    sprintf(msg, "%d", lightLevel);
    server.send(200, "text/plain", msg);
  }
  if (server.arg("sensor") == "temperature") {
    char msg[10];
    sprintf(msg, "%0.2f", temperature);
    server.send(200, "text/plain", msg);
  } else {
    server.send(404, "text/plain", "ERROR: Sensor not found.");
  }
}

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

  switches["light"]["state"] = lvLights->state();
  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["lightLevel"] = lightLevel;
  sensors["temperature"] = temperature;
  sensors["humidity"] = humidity;
  doc["debug"] = debug;
  char timeString[20];
  struct tm *timeinfo = localtime(&now);
  strftime (timeString,20,"%D %T",timeinfo);
  doc["time"] = timeString;

  size_t jsonDocSize = measureJsonPretty(doc);
  if (jsonDocSize > JSON_SIZE) {
    char msg[40];
    sprintf(msg, "ERROR: JSON message too long, %d", jsonDocSize);
    server.send(500, "text/plain", msg);
  } else {
    String httpResponse;
    serializeJsonPretty(doc, httpResponse);
    server.send(500, "text/plain", httpResponse);
  }
}


time_t prevTime = 0;;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  time_t now = time(nullptr);
  if ( now != prevTime ) {
    if ( debug ) {
      syslog.logf(LOG_INFO, "seconds changed, now: %lld, prev: %lld", now, prevTime);
    }
    if ( now % 5 == 0 ) {
      if ( vemlSensorFound ) {
	lightLevel = veml.readLux();
      }
      syslog.appName(THERMO_APPNAME);
      if ( thermoSensorFound ) {
	if (! am2315.readTemperatureAndHumidity(&temperature, &humidity)) {
	  syslog.log(LOG_INFO, "Failed to read data from AM2315");
	}
      }
      // convert the temperature to F
      temperature = ((temperature * 9) / 5 + 32);
      syslog.appName(SYSTEM_APPNAME);
    }
    prevTime = now;
  } 
}
