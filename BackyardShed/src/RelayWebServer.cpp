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
#include "Adafruit_MCP23X17.h"

#include <my_relay.h>
#include <my_reed.h>
#include <my_veml.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

// This device info
#define APP_NAME "switch"
#define JSON_SIZE 1500
#define MYTZ TZ_America_Los_Angeles

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

Veml veml;

// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;
const int REED_PIN = 15;

int debug = 0;
StaticJsonDocument<200> doc;
Adafruit_MCP23X17 mcp;
Vector<IrrigationRelay*> IrrigationZones;
IrrigationRelay * storage_array[8];

ReedSwitch * shedDoor = new ReedSwitch(REED_PIN, &mcp);

void handleDebug() {
  if (server.arg("level") == "status") {
    char msg[40];
    sprintf(msg, "Debug level: %d", debug);
    server.send(200, "text/plain",msg);
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

void handleZones() {
  String zones;

  bool first = true;
  for (IrrigationRelay * relay : IrrigationZones) {
    if ( first ) {
      zones = relay->name;
      first = false;
    } else {
      zones = zones + ", " + relay->name;
    }
  }
  server.send(200, "text/plain", zones);
}

void handleDoor() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", shedDoor->status() ? "1" : "0");
    return;
  } 
  server.send(404, "text/plain", "ERROR: uknonwn state command");
}

void handleSensors() {
  if (server.arg("sensor") == "light") {
    char msg[10];
    sprintf(msg, "%d", veml.readLux());
    server.send(200, "text/plain", msg);
  } else if (server.arg("sensor") == "door") {
    server.send(200, "text/plain", shedDoor->status() ? "1" : "0");
  }
}

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  char suppliedZone[15];
  server.arg("zone").toCharArray(suppliedZone, 15);
  bool matchFound = false;

  JsonObject switches = doc.createNestedObject("switches");
  JsonObject sensors = doc.createNestedObject("sensors");
  for (IrrigationRelay * relay : IrrigationZones) {
    if (server.arg("zone") == relay->name) {
      matchFound = true;
      switches[relay->name]["State"] = relay->state();
      switches[relay->name]["Override"] = relay->scheduleOverride;
      switches[relay->name]["Soil Moisture Level"] = relay->soilMoistureLevel;
      switches[relay->name]["Soil Moisture Percentage"] = relay->soilMoisturePercentage;
      switches[relay->name]["Time Left"] = relay->timeLeftToRun;
      switches[relay->name]["Last Run Time"] = relay->prettyOnTime;
      switches[relay->name]["Next Run Time"] = relay->nextTimeToRun;
      char weekSchedule[8];
      relay->getWeekSchedule(weekSchedule);
      switches[relay->name]["Week Schedule"] = weekSchedule;
    }
  } 

  if (!matchFound) {
    char msg[60];
    sprintf(msg, "ERROR: irrigation zone %s not found", suppliedZone);
    server.send(404, "text/plain", msg);
    return;
  }

  sensors["Door Status"] = shedDoor->state();
  sensors["Light Level"] = veml.readLux();
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
      if (server.arg("override") == "true") {
	      relay->setScheduleOverride(true);
	      syslog.logf(LOG_INFO, "Schedule disabled for irrigation zone %s on by API request", relay->name);
	      server.send(200, "text/plain");
	      return;
      } else if (server.arg("override") == "false") {
	      relay->setScheduleOverride(false);
	      syslog.logf(LOG_INFO, "Schedule enabled for irrigation zone %s on by API request", relay->name);
	      server.send(200, "text/plain");
	      return;
      } else if (server.arg("state") == "status") {
	server.send(200, "text/plain", relay->status() ? "1" : "0");
	return;
      } else if (server.arg("state") == "on") {
	      relay->switchOn();
	      syslog.logf(LOG_INFO, "Turned irrigation zone %s on by API request for %ds", relay->name, relay->runTime);
	      server.send(200, "text/plain");
	      return;
      } else if (server.arg("state") == "off") {
	      relay->switchOff();
	      syslog.logf(LOG_INFO, "Turned irrigation zone %s off by API request", relay->name);
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
  Wire.begin(GPIO0_PIN, GPIO2_PIN);
  if (!mcp.begin_I2C()) {
    syslog.log(LOG_INFO, "ERROR: MCP setup failed");
  }

  if ( ! veml.setup() ) {
    syslog.log(LOG_INFO, "ERROR: veml setup failed");
  }
  
  // setup the reed switch on the shed door
  shedDoor->setup("shed_door");

  IrrigationZones.setStorage(storage_array);

  //TODO: add starttimes to status
  //maybe the mcp reference can be passed just once?
  //address, backwards?, Start, Mins, Schedule, EveryOtherDay?
  IrrigationRelay * irz1 = new IrrigationRelay("patio_pots",  7, true, "7:00",  3, false, &mcp);
  IrrigationRelay * irz2 = new IrrigationRelay("cottage",     6, true, "7:15", 15, true,  &mcp);
  IrrigationRelay * irz3 = new IrrigationRelay("south_fence", 5, true, "7:30",  5, false, &mcp);
  IrrigationRelay * irz4 = new IrrigationRelay("hill",        4, true, "7:45", 20, false,  &mcp);
  IrrigationRelay * irz5 = new IrrigationRelay("back_fence",  2, true, "8:15", 15, false,  &mcp);
  IrrigationRelay * irz6 = new IrrigationRelay("north_fence", 1, true, "8:30", 15, true,  &mcp);
// IrrigationRelay * irz7 = new IrrigationRelay("garden",      3, true, "6:00", 8, &mcp);
// irz7->setStartTimeFromString(                                       "10:00");
// irz7->setStartTimeFromString(                                       "14:00");
// irz7->setStartTimeFromString(                                       "18:00");
  irz1->setup(); IrrigationZones.push_back(irz1);
  irz2->setup(); IrrigationZones.push_back(irz2);
  irz3->setup(); IrrigationZones.push_back(irz3);
  irz4->setup(); IrrigationZones.push_back(irz4);
  irz5->setup(); IrrigationZones.push_back(irz5);
  irz6->setup(); IrrigationZones.push_back(irz6);

  // PatioPots irz1->setSoilMoistureSensor(0x48, 0, 86); // i2c address, pin, % to run
  // Cottage irz2->setSoilMoistureSensor(0x48, 1, 86); // i2c address, pin, % to run
  // Hill irz3->setSoilMoistureSensor(0x48, 3, 86); // i2c address, pin, % to run
  // South Fence irz4->setSoilMoistureSensor(0x48, 2, 86); // i2c address, pin, % to run
  // Back fence irz5->setSoilMoistureSensor(0x4b, 2, 86); // i2c address, pin, % to run
  // North Fence irz6->setSoilMoistureSensor(0x4b, 1, 86); // i2c address, pin, % to run
  // Garden irz7->setSoilMoistureSensor(0x4b, 3, 86); // i2c address, pin, % to run
  // Zone8 irz8->setSoilMoistureSensor(0x4b, 0, 86); // i2c address, pin, % to run

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/irrigation", handleIrrigation);
  server.on("/zones", handleZones);
  server.on("/door", handleDoor);
  server.on("/sensors", handleSensors);
  server.on("/status", handleStatus);

  server.begin();
  Serial.println("End of setup");
}

time_t prevTime = 0;;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  time_t now = time(nullptr);
  if (prevTime != now) {
    if ( shedDoor->handle() ) {
      syslog.logf(LOG_INFO, "%s %s", shedDoor->name, shedDoor->state());
    }
  }
  prevTime = now;


  for (IrrigationRelay * relay : IrrigationZones) {
    if ( relay->handle() ) {
      syslog.logf(LOG_INFO, "%s %s; soil moisture: %f%%", relay->name, relay->state(), relay->soilMoisturePercentage);
    }
  }

}


