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

  // Soil Moisture Sensor addresses: 0x48, 0x49, 0x4a, 0x4b
  //TODO: add starttimes to status

  // zone1 startTimes: 7:00, 11:00, 15:00, 19:00
  const char *zoneNames[2] = { "patio_pots", "cottage" };
  syslog.logf(LOG_INFO, "first zone: %s", zoneNames[0]);

  // Pots and Plants Irrigation
  IrrigationRelay * irz1 = new IrrigationRelay(7, &mcp);
  irz1->setBackwards();
  irz1->setup("patio_pots");
  irz1->setRuntime(3*60);
  // Disabled 1/10/22
  //irz1->setStartTime(7,0); // hour, minute
  //irz1->setStartTime(15,0); // hour, minute
//  irz1->setSoilMoistureSensor(0x48, 0, 86); // i2c address, pin, % to run
//  irz1->setSoilMoistureLimits(660, 218); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 1 %s setup done", irz1->name);
  IrrigationZones.push_back(irz1);

  IrrigationRelay * irz2 = new IrrigationRelay(6, &mcp);
  irz2->setBackwards();
  irz2->setup("cottage");
  // Disabled 11/21
//  irz2->setStartTime(7,30); // hour, minute
//  irz2->setStartTime(15,30); // hour, minute
  irz2->setRuntime(10*60);
//  irz2->setSoilMoistureSensor(0x48, 1, 86); // i2c address, pin, % to run
//  irz2->setSoilMoistureLimits(430, 179); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 2 %s setup done", irz2->name); 
  IrrigationZones.push_back(irz2);

  // South Fence
  IrrigationRelay * irz3 = new IrrigationRelay(5, &mcp);
  irz3->setBackwards();
  irz3->setup("south_fence");
  irz3->setRuntime(5*60);
  // Disabled 1/10/22
  // irz3->setStartTime(7,10); // hour, minut
//  irz3->setStartTime(11,10); // hour, minute
  // irz3->setStartTime(15,10); // hour, minute
//  irz3->setStartTime(19,10); // hour, minute
//  irz3->setSoilMoistureSensor(0x48, 2, 86); // i2c address, pin, % to run
//  irz3->setSoilMoistureLimits(430, 179); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 3 %s setup done", irz3->name); 
  IrrigationZones.push_back(irz3);

  // Hill
  IrrigationRelay * irz4 = new IrrigationRelay(4, &mcp);
  irz4->setBackwards();
  irz4->setup("hill");
  // Disabled 11/21
//  irz4->setStartTime(8,0); // hour, minute
//  irz4->setStartTime(16,0); // hour, minute
  irz4->setRuntime(10*60);
//  irz4->setSoilMoistureSensor(0x48, 3, 86); // i2c address, pin, % to run
//  irz4->setSoilMoistureLimits(430, 179); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 4 %s setup done", irz4->name); 
  IrrigationZones.push_back(irz4);

  // Garden
  IrrigationRelay * irz5 = new IrrigationRelay(3, &mcp);
  irz5->setBackwards();
  irz5->setup("garden");
  // Disabled 11/21
//  irz5->setStartTime(6,0); // hour, minute
//  irz5->setStartTime(10,0); // hour, minute
//  irz5->setStartTime(14,0); // hour, minute
//  irz5->setStartTime(18,0); // hour, minute
//  irz5->setRuntime(8*60);
//  irz5->setSoilMoistureSensor(0x4b, 3, 86); // i2c address, pin, % to run
//  irz5->setSoilMoistureLimits(430, 179); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 5 %s setup done", irz5->name); 
  IrrigationZones.push_back(irz5);

  // Back fence
  IrrigationRelay * irz6 = new IrrigationRelay(2, &mcp);
  irz6->setBackwards();
  irz6->setup("back_fence");
  // Disabled 1/10/22
  //irz6->setStartTime(9,0); // hour, minute
  //irz6->setStartTime(19,0); // hour, minute
  irz6->setRuntime(10*60);
//  irz6->setSoilMoistureSensor(0x4b, 2, 86); // i2c address, pin, % to run
//  irz6->setSoilMoistureLimits(430, 179); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 6 %s setup done", irz6->name); 
  IrrigationZones.push_back(irz6);

  // North Fence
  IrrigationRelay * irz7 = new IrrigationRelay(1, &mcp);
  irz7->setBackwards();
  irz7->setup("north_fence");
  // Disabled 1/10/22
  //irz7->setStartTime(9,20); // hour, minute
  //irz7->setStartTime(19,20); // hour, minute
  irz7->setRuntime(10*60);
//  irz7->setSoilMoistureSensor(0x4b, 1, 86); // i2c address, pin, % to run
//  irz7->setSoilMoistureLimits(430, 179); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 7 %s setup done", irz7->name); 
  IrrigationZones.push_back(irz7);

  // Zone8
  IrrigationRelay * irz8 = new IrrigationRelay(0, &mcp);
  irz8->setBackwards();
  irz8->setup("zone8");
//  irz8->setSoilMoistureSensor(0x4b, 0, 86); // i2c address, pin, % to run
//  irz8->setSoilMoistureLimits(430, 179); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 8 %s setup done", irz8->name); 
  IrrigationZones.push_back(irz8);

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/irrigation", handleIrrigation);
  server.on("/zones", handleZones);
  server.on("/door", handleDoor);
  server.on("/sensors", handleSensors);
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


