#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include "Adafruit_MCP23X17.h"

#include <my_relay.h>
#include <my_motion.h>
#include <my_distance.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

// This device info
#define APP_NAME "garagedoor"
#define JSON_SIZE 500
#define MYTZ TZ_America_Los_Angeles

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;
bool scanI2CMode = false;

/*
   INPUT  OUTPUT
   D1 OK     OK   i2c clock i2c clock
   D2 OK     OK   i2c data  i2c data
   D3        OK             garage door relay
   D4        OK             led open
   D5 OK     OK   echo      led closed
   D6 OK     OK   trig      reed open
   D7 OK     OK             reed closed
*/

const uint8_t DISTANCE_ECHO_PIN = D5;
const uint8_t DISTANCE_TRIG_PIN = D6;

// MCP23017 PINS
const uint8_t LIGHT_RELAY_PIN = 0;
const uint8_t MOTION_FRONT_PIN = 1;
const uint8_t MOTION_BACK_PIN = 2;

const uint8_t DOOR_RELAY_PIN = 3;
const uint8_t LED_OPEN_PIN = 4;
const uint8_t LED_CLOSED_PIN = 5;
const uint8_t REED_OPEN_PIN = 6;
const uint8_t REED_CLOSED_PIN = 7;

Adafruit_MCP23X17 mcp;
Relay * overheadLight = new Relay(LIGHT_RELAY_PIN, &mcp);

GarageDoorRelay * garageDoor = new GarageDoorRelay(DOOR_RELAY_PIN, REED_OPEN_PIN, REED_CLOSED_PIN, LED_OPEN_PIN, LED_CLOSED_PIN, &mcp);

MOTION * frontMotion = new MOTION(MOTION_FRONT_PIN, &mcp);
MOTION * backMotion = new MOTION(MOTION_BACK_PIN, &mcp);

DISTANCE * distance = new DISTANCE(DISTANCE_ECHO_PIN, DISTANCE_TRIG_PIN);

void setup() {
  Serial.begin(115200);

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
  syslog.log(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  // set I2C pins (SDA, CLK)
  Wire.begin(D2,D1);
  if (!mcp.begin_I2C()) {
    syslog.log(LOG_INFO, "ERROR: MCP setup failed");
  }

  garageDoor->setup("garagedoor");
  overheadLight->setup("light");
  frontMotion->setup("front_motion");
  backMotion->setup("back_motion");
  distance->setup("distance");

  // Start the server
  // Start the server
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/scani2c", handleScanI2C);
  server.on("/door", handleDoor);
  server.on("/light", handleLight);
  server.begin();
}

void handleScanI2C() {
  if (server.arg("state") == "on") {
    server.send(200, "text/plain");
    scanI2CMode = true;
    syslog.log(LOG_INFO, "Turned on I2C Scanning");
  } else if (server.arg("state") == "off") {
    server.send(200, "text/plain");
    scanI2CMode = false;
    syslog.log(LOG_INFO, "Turned off I2C Scanning");
  } else {
    server.send(404, "text/plain", "ERROR: unknown scani2c command");
  }
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
  switches[garageDoor->name]["state"] = garageDoor->state();
  switches[overheadLight->name]["state"] = overheadLight->state();

  JsonObject sensors = doc.createNestedObject("sensors");
  sensors[distance->name]["inches"] = distance->inches;
  sensors[frontMotion->name]["state"] = frontMotion->activity();
  sensors[backMotion->name]["state"] = backMotion->activity();

  doc["i2cScan"] = scanI2CMode;
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

void handleDoor() {
  if (server.arg("command") == "status") {
    server.send(200, "text/plain", garageDoor->state());
  } else if (server.arg("command") == "operate") {
    syslog.log(LOG_INFO, "Operating Garage Door");
    garageDoor->operate();
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn door command");
  }
}

void handleLight() {
  if (server.arg("command") == "status") {
    server.send(200, "text/plain", overheadLight->state());
  } else if (server.arg("command") == "on") {
    syslog.log(LOG_INFO, "Turning light On");
    overheadLight->switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("command") == "off") {
    syslog.log(LOG_INFO, "Turning light Off");
    overheadLight->switchOff();
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn light command");
  }
}

bool personActive = false;
time_t now, prevTime = 0;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  prevTime = now;
  now = time(nullptr);

  // only check every second
//  if (  now == prevTime ) { 
//    mcp.pinMode(LED_OPEN_PIN, OUTPUT);
//    mcp.pinMode(LED_CLOSED_PIN, OUTPUT);
//    if ( debug ) { 
//      if ( now % 2 == 0 ) {
//	mcp.digitalWrite(LED_OPEN_PIN, LOW);
//	mcp.digitalWrite(LED_CLOSED_PIN, LOW);
//      } else {
//	mcp.digitalWrite(LED_OPEN_PIN, HIGH);
//	mcp.digitalWrite(LED_CLOSED_PIN, HIGH);
//      }
//    return;
//    }
//  }

  if ( scanI2CMode ) {
    scanI2C();
    scanI2CMode = false;
  }

  distance->handle();

  if (garageDoor->handle()) {
    syslog.logf(LOG_INFO, "Garage door %s", garageDoor->state());
  }

  frontMotion->handle();
  backMotion->handle();
  if (! personActive) {
    if (frontMotion->activity() || backMotion->activity()) {
      syslog.logf(LOG_INFO, "Motion detected in %s, turning light on", frontMotion->activity() ? "front" : "back" );
      overheadLight->switchOn();
      personActive = true;
    }
  } else {
    if (! frontMotion->activity() && ! backMotion->activity()) {
      syslog.log(LOG_INFO, "No motion detected, turning light off");
      overheadLight->switchOff();
      personActive = false;
    } 
  }
} //main loop

void scanI2C() {
  byte error, address;
  int nDevices;

  syslog.logf(LOG_INFO, "Starting I2C Scan"); 

  nDevices = 0;
  for(address = 0; address <= 127; address++ ) 
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      char hexaddr[10];
      sprintf(hexaddr, "0x%02x", address);
      syslog.logf(LOG_INFO, "I2C device found at address %s", hexaddr);

      nDevices++;
    }
    else if (error==4) 
    {
      char hexaddr[10];
      sprintf(hexaddr, "0x%02x", address);
      syslog.logf(LOG_INFO, "Unknown error at address %s", hexaddr);
    }    
  }

  if (nDevices == 0)
    syslog.log(LOG_INFO, "No I2C devices found\n");
  else
    syslog.log(LOG_INFO, "I2C scan done\n");

}
