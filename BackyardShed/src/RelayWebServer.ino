#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Vector.h>

#include <my_relay.h>
//#include <my_veml.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#include <Wire.h>
#include "Adafruit_MCP23017.h"

// This device info
#define APP_NAME "switch"
#define JSON_SIZE 500
#define MYTZ TZ_America_Los_Angeles
// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;
int irrigationAction = 0;

StaticJsonDocument<200> doc;

Adafruit_MCP23017 mcp;
Vector<IrrigationRelay*> IrrigationZones;

IrrigationRelay * storage_array[8];

ScheduleRelay * schedTest1 = new ScheduleRelay(TX_PIN);
DuskToDawnScheduleRelay * d2dSchedTest1 = new DuskToDawnScheduleRelay(TX_PIN);

//Veml yveml;

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

  // setup the light sensor
//  if (!myveml.setup()) {
//    syslog.log(LOG_INFO, "VEML setup failed");
//  }

  schedTest1->setup("schedule_test");
  schedTest1->setOnOffTimes(10, 42, 10, 43);
  d2dSchedTest1->setup("dusk_to_dawn_test");
  if (!d2dSchedTest1->setVemlLightSensor()) {
    syslog.log(LOG_INFO, "ERROR: setVemlLightSensor() failed");
  }

  IrrigationZones.setStorage(storage_array);

  // Garden Irrigation
  IrrigationRelay * irz1 = new IrrigationRelay(0, &mcp, true);
  irz1->setup("garden");
  irz1->setRuntime(1);
  irz1->setStartTime(17,1); // hour, minute
  irz1->setSoilMoistureSensor(0x48, 0, 86); // i2c address, pin, % to run
  irz1->setSoilMoistureLimits(465, 228); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 1 %s setup done", irz1->name);
  IrrigationZones.push_back(irz1);

  // Pots and Plants Irrigation
  IrrigationRelay * irz2 = new IrrigationRelay(1, &mcp, true);
  irz2->setup("patio_pots");
  irz2->setRuntime(1);
  irz2->setStartTime(17,2); // hour, minute
  irz2->setSoilMoistureSensor(0x48, 1, 86); // i2c address, pin, % to run
  irz2->setSoilMoistureLimits(727, 310); // dry, wet
  syslog.logf(LOG_INFO, "irrigation Zone 2 %s setup done", irz2->name); 
  IrrigationZones.push_back(irz2);

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/irrigation", handleIrrigation);
  server.on("/d2d", handleD2D);

  server.begin();
}

void handleD2D() {
  if (server.arg("level") == "status") {
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
  JsonObject sensors = doc.createNestedObject("sensors");
  for (IrrigationRelay * relay : IrrigationZones) {
   switches[relay->name]["state"] = relay->on ? "on" : "off";
   switches[relay->name]["soilMoistureLevel"] = relay->soilMoistureLevel;
   switches[relay->name]["soilMoisturePercentage"] = relay->soilMoisturePercentage;
  } 

  sensors["luxLevel"] = d2dSchedTest1->lightLevel;
  doc["irrigationReturnCode"] = irrigationAction;
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
	server.send(200, "text/plain", relay->on ? "1" : "0");
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

  time_t now = time(nullptr);
  if ( now != prevTime ) {
    if ( now % 5 == 0 ) {

      irrigationAction = schedTest1->handle();
      if ( irrigationAction == 1 ) {
	syslog.logf(LOG_INFO, "scheduled light '%s' turned on", schedTest1->name);
      } 
      if ( irrigationAction == 2 ) {
	syslog.logf(LOG_INFO, "scheduled light '%s' turned off", schedTest1->name);
      } 

      for (IrrigationRelay * relay : IrrigationZones) {
	irrigationAction = relay->handle();
	if ( irrigationAction == 1 ) {
	  syslog.logf(LOG_INFO, "scheduled irrigation started for zone %s", relay->name);
	} 
	if ( irrigationAction == 2 ) {
	  syslog.logf(LOG_INFO, "scheduled irrigation stopped for zone %s", relay->name);
	}
	if ( prevIrrigationAction != 3 && irrigationAction == 3 ) {
	  syslog.logf(LOG_INFO, "scheduled irrigation for zone %s not started because the soil's wet", relay->name);
	}
	if ( prevIrrigationAction != 4 && irrigationAction == 4 ) {
	  syslog.logf(LOG_INFO, "scheduled irrigation stopped for zone %s because the soil's wet", relay->name);
	}
      }

      irrigationAction = d2dSchedTest1->handle();
      if ( irrigationAction == 1 ) {
	syslog.logf(LOG_INFO, "D2D light '%s' turned on", d2dSchedTest1->name);
      } 
      if ( irrigationAction == 2 ) {
	syslog.logf(LOG_INFO, "D2D light '%s' turned off", d2dSchedTest1->name);
      } 
    }
  }
  prevTime = now;

  server.handleClient();
}


