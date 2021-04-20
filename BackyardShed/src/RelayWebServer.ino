#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <Vector.h>
#include <Wire.h>
#include "Adafruit_MCP23017.h"

#include <my_relay.h>
#include <my_reed.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

// This device info
#define APP_NAME "switch"
#define JSON_SIZE 750
#define MYTZ TZ_America_Los_Angeles

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;

const int REED_PIN = 15;

int debug = 0;

StaticJsonDocument<200> doc;

Adafruit_MCP23017 mcp;
Vector<IrrigationRelay*> IrrigationZones;

IrrigationRelay * storage_array[8];

ReedSwitch * shedDoor = new ReedSwitch(REED_PIN, &mcp);

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
  syslog.log(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");
  
  // set I2C pins (SDA, SDL)
  Wire.begin(GPIO2_PIN, GPIO0_PIN);
  mcp.begin();      // use default address 0

  // setup the reed switch on the shed door
  shedDoor->setup("shed_door");

  IrrigationZones.setStorage(storage_array);

  // Garden Irrigation
  IrrigationRelay * irz1 = new IrrigationRelay(0, &mcp);
  irz1->setBackwards();
  irz1->setup("garden");
  irz1->setRuntime(1);
  irz1->setStartTime(18,2); // hour, minute
  irz1->setSoilMoistureSensor(0x48, 0, 86); // i2c address, pin, % to run
  irz1->setSoilMoistureLimits(465, 228); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 1 %s setup done", irz1->name);
  IrrigationZones.push_back(irz1);

  // Pots and Plants Irrigation
  IrrigationRelay * irz2 = new IrrigationRelay(1, &mcp);
  irz2->setBackwards();
  irz2->setup("patio_pots");
  irz2->setEveryDayOff();
  irz2->setSpecificDayOn(0);
  irz2->setSpecificDayOn(3);
  irz2->setSpecificDayOn(5);
  irz2->setRuntime(10);
  irz2->setStartTime(18,3); // hour, minute
  irz2->setSoilMoistureSensor(0x48, 1, 86); // i2c address, pin, % to run
  irz2->setSoilMoistureLimits(727, 310); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 2 %s setup done", irz2->name); 
  IrrigationZones.push_back(irz2);

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/irrigation", handleIrrigation);

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
  JsonObject sensors = doc.createNestedObject("sensors");
  for (IrrigationRelay * relay : IrrigationZones) {
    switches[relay->name]["State"] = relay->state();
    switches[relay->name]["Soil Moisture Level"] = relay->soilMoistureLevel;
    switches[relay->name]["Soil Moisture Percentage"] = relay->soilMoisturePercentage;

    switches[relay->name]["Time Left"] = relay->timeLeftToRun;
    switches[relay->name]["Last Run Time"] = relay->prettyOnTime;
    switches[relay->name]["Next Run Time"] = relay->nextTimeToRun;
  } 

  sensors["Door Status"] = shedDoor->state();
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

void handleIrrigation() {
  bool matchFound = false;
  char suppliedZone[15];
  server.arg("zone").toCharArray(suppliedZone, 15);

  for (IrrigationRelay * relay : IrrigationZones) {
    if (debug) {
      syslog.logf(LOG_INFO, "DEBUG: irrigation handling: zone arg: %s, relay name: %s", suppliedZone, relay->name);
    }

    if (server.arg("zone") == relay->name) {
      matchFound = true;
      if (server.arg("state") == "status") {
	server.send(200, "text/plain", relay->status() ? "1" : "0");
	return;
      } else if (server.arg("state") == "on") {
	syslog.logf(LOG_INFO, "Turning irrigation zone %s on by API request at %ld", relay->name, relay->onTime);
	relay->switchOn();
	server.send(200, "text/plain");
	return;
      } else if (server.arg("state") == "off") {
	syslog.logf(LOG_INFO, "Turning irrigation zone %s off by API request at %ld", relay->name, relay->onTime);
	relay->switchOff();
	server.send(200, "text/plain");
	return;
      } else {
	char msg[40];
	sprintf(msg, "ERROR: state command not specified for %s", relay->name);
	server.send(404, "text/plain", msg);
      }
    }
  }
  if (!matchFound) {
    char msg[60];
    sprintf(msg, "ERROR: irrigation zone %s not found", suppliedZone);
    server.send(404, "text/plain", msg);
    return;
  }

  server.send(404, "text/plain", "ERROR: Unknown irrigation command");
}

time_t prevTime = 0;;
int prevIrrigationAction = 0;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  if ( shedDoor->handle() ) {
    syslog.logf(LOG_INFO, "%s %sed", shedDoor->name, shedDoor->state());
  }

  for (IrrigationRelay * relay : IrrigationZones) {
    if ( relay->handle() ) {
      syslog.logf(LOG_INFO, "%s %s; soil moisture: %f%%", relay->name, relay->state(), relay->soilMoisturePercentage);
    }
  }
}


