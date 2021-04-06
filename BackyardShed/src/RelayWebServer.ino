#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Array.h>

#include <my_relay.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#include <Wire.h>
#include "Adafruit_MCP23017.h"

#include <SPI.h>
//#include <Adafruit_ADS1X15.h>

#define MYTZ TZ_America_Los_Angeles

// This device info
#define APP_NAME "switch"
#define JSON_SIZE 300

ESP8266WebServer server(80);
//typedef Array<int,7> Elements;
//const int irrigationDays[7] = {0,2,4,6};

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;

// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;

StaticJsonDocument<200> doc;

Adafruit_MCP23017 mcp;
//McpRelay irrigationZone1(1, &mcp);
//Relay irrigationZone1(1, &mcp);
IrrigationRelay irrigationZone1(1, &mcp);
Relay light(1);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting up");

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to network...");
    delay(500);
  }

  char msg[40];
  sprintf(msg, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");
  
  // set I2C pins (SDA, SDL)
  Wire.begin(GPIO2_PIN, GPIO0_PIN);
  mcp.begin();      // use default address 0

  // Setup the Relay
  irrigationZone1.setup();
  char irrigationName_1[20] = "garden";
  irrigationZone1.setName(irrigationName_1);
  syslog.logf(LOG_INFO, "irrigation Zone 1 name: %s", irrigationZone1.name);

  irrigationZone1.setSoilMoisture(0,86); // pin for analog read, percentage to run at
  irrigationZone1.setI2cSoilMoistureSensor(); // use the i2c interface to use the ads class
  irrigationZone1.setSoilMoistureLimits(465, 228);

  light.setup();

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/irrigation_1", handleIrrigationZone1);
  server.on("/light", handleLight);
  server.on("/status", handleStatus);

  server.begin();
}

void handleDebug() {
  if (server.arg("level") == "status") {
    char msg[40];
    sprintf(msg, "Debug level: %d", debug);
    server.send(200, "text/plain", msg);
  } else if (server.arg("level") == "0") {
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
  } else {
    server.send(404, "text/plain", "ERROR: unknown debug command");
  }
}

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;

  JsonObject switches = doc.createNestedObject("switches");
  switches["irrigation"]["state"] = irrigationZone1.state();
  switches["light"]["state"] = irrigationZone1.state();

  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["soilMoistureLevel"] = irrigationZone1.soilMoistureLevel;
  sensors["soilMoisturePercentage"] = irrigationZone1.soilMoisturePercentage;
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

void handleIrrigationZone1() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", irrigationZone1.on ? "1" : "0");
  } else if (server.arg("state") == "on") {
    syslog.logf(LOG_INFO, "Turning irrigation on at %ld", irrigationZone1.onTime);
    irrigationZone1.switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    syslog.logf(LOG_INFO, "Turning irrigation off at %ld", irrigationZone1.offTime);
    irrigationZone1.switchOff();
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown light command");
  }
}

void handleLight() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", light.on ? "1" : "0");
  } else if (server.arg("state") == "on") {
    syslog.logf(LOG_INFO, "Turning light on at %ld", light.onTime);
    light.switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    syslog.logf(LOG_INFO, "Turning light off at %ld", light.offTime);
    light.switchOff();
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown light command");
  }
}

time_t prevTime = 0;;
void loop() {
  ArduinoOTA.handle();

  time_t now = time(nullptr);
  if ( now != prevTime ) {
    if ( now % 5 == 0 ) {
      irrigationZone1.handle();
    }
  }
  prevTime = now;

  server.handleClient();
}


