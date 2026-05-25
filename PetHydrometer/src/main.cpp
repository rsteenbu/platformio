#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <Vector.h>
#include <Wire.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#include <my_lcd.h>
#include <my_relay.h>
#include <my_motion.h>
#include <my_dht.h>
#include <Adafruit_Sensor.h>

#include "config_default.h"
#include "pet_config.h"

ESP8266WebServer server(80);

#define SYSTEM_APPNAME "arduino"
#define MISTER_APPNAME "mister"
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, SYSTEM_APPNAME, LOG_LOCAL0);

Vector<myDHT*> DHTSensors;
myDHT* storage_array[8];

int debug = DEBUG;
char msg[40];
LCD * lcd = new LCD();
MOTION * motion = new MOTION(D7); 
IrrigationRelay * Mister = new IrrigationRelay(D3);

void handleDebug() {
  String levelArg = server.arg("level");
  if (levelArg == "status") {
    char msg[40];
    sprintf(msg, "Debug level: %d", debug);
    server.send(200, "text/plain", msg);
  } else if (levelArg == "0" || levelArg == "1" || levelArg == "2") {
    debug = levelArg.toInt();
    syslog.logf(LOG_INFO, "Debug level %d", debug);
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown debug command");
  }
}

void handleMister() {
  if (server.arg("state") == "on") {
    Mister->switchOn();
    syslog.appName(MISTER_APPNAME);
    syslog.logf(LOG_INFO, "Turned %s on for %ds", Mister->name, Mister->runTime);
    syslog.appName(SYSTEM_APPNAME);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    Mister->switchOff();
    syslog.appName(MISTER_APPNAME);
    syslog.logf(LOG_INFO, "Turned %s off", Mister->name);
    syslog.appName(SYSTEM_APPNAME);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", Mister->on ? "1" : "0");
  } else if (server.arg("addTime") != "") {
    int timeToAdd = server.arg("addTime").toInt();
    Mister->addTimeToRun(timeToAdd);
    syslog.appName(MISTER_APPNAME);
    syslog.logf(LOG_INFO, "Added %d seconds, total runtime now %ds, time left %ds", timeToAdd, Mister->runTime, Mister->getSecondsLeft());
    syslog.appName(SYSTEM_APPNAME);
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown mister command");
  }
}

void handleDisplay() {
  if (server.arg("state") == "on") {
    server.send(200, "text/plain");
    lcd->setBackLight(true);
    syslog.log(LOG_INFO, "Turned LCD Display on");
  } else if (server.arg("state") == "off") {
    server.send(200, "text/plain");
    lcd->setBackLight(false);
    syslog.log(LOG_INFO, "Turned LCD Display off");
  } else {
    server.send(404, "text/plain", "ERROR: unknown display command");
  }
}

void handleSensors() {
  String tempArg = server.arg("temp");
  String humidArg = server.arg("humidity");
  for (myDHT* sensor : DHTSensors) {
    char msg[10];
    if (tempArg == "left" && strcmp(sensor->sensorName, "leftDHT") == 0) {
      sprintf(msg, "%0.2f", sensor->temp);
      server.send(200, "text/plain", msg);
      return;
    } else if (tempArg == "right" && strcmp(sensor->sensorName, "rightDHT") == 0) {
      sprintf(msg, "%0.2f", sensor->temp);
      server.send(200, "text/plain", msg);
      return;
    } else if (humidArg == "left" && strcmp(sensor->sensorName, "leftDHT") == 0) {
      sprintf(msg, "%0.2f", sensor->humid);
      server.send(200, "text/plain", msg);
      return;
    } else if (humidArg == "right" && strcmp(sensor->sensorName, "rightDHT") == 0) {
      sprintf(msg, "%0.2f", sensor->humid);
      server.send(200, "text/plain", msg);
      return;
    }
  }
  server.send(404, "text/plain", "ERROR: Sensor not found.");
}

void handlePrometheus() {

  static size_t const BUFSIZE = 1024;
  static char const *response_template =
      "# HELP " PET_NAME "_air_humidity_percent Air humidity.\n"
      "# TYPE " PET_NAME "_air_humidity_percent gauge\n"
      "# UNIT " PET_NAME "_air_humidity_percent %%\n"
      PET_NAME "_air_humidity_percent{petName=\"" PET_NAME "\",type=\"humidity\",sensorName=\"%s\"} %f\n"
      "# HELP " PET_NAME "_air_temperature_fahrenheit Air temperature.\n"
      "# TYPE " PET_NAME "_air_temperature_fahrenheit gauge\n"
      "# UNIT " PET_NAME "_air_temperature_fahrenheit \u00B0F\n"
      PET_NAME "_air_temperature_fahrenheit{petName=\"" PET_NAME "\",type=\"temperature\",sensorName=\"%s\"} %f\n"
      ;
  char response[BUFSIZE] = "";
  size_t offset = 0;
  for (myDHT* sensor : DHTSensors) {
    offset += snprintf(response + offset, BUFSIZE - offset, response_template,
                       sensor->sensorName, sensor->humid, sensor->sensorName, sensor->temp);
  }

    server.send(200, "text/plain; charset=utf-8", response);
}

void handleStatus() {
  time_t now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

  switches["mister"]["state"] = Mister->state();
  switches["mister"]["active"] = Mister->active;
  switches["mister"]["RunTime (s)"] = Mister->runTime;
  switches["mister"]["Time Left"] = Mister->timeLeftToRun;
  switches["mister"]["Next Run Time"] = Mister->nextTimeToRun;
  switches["mister"]["Last Run Time"] = Mister->prettyOnTime;

  JsonObject sensors = doc.createNestedObject("sensors");

  for (myDHT* sensor : DHTSensors) {
    sensors[sensor->sensorName]["humidity"] = sensor->humid;
    sensors[sensor->sensorName]["temperature"] = sensor->temp;
  }

  doc["LCD Backlight Status"] = lcd->state;
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
    server.send(200, "text/plain", httpResponse);
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // set I2C pins (SDA, CLK)
  Wire.begin(D2, D1);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to network...");
    delay(500);
  }

  sprintf(msg, "%s Alive! at IP: %s", DEVICE_HOSTNAME, (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/mister", handleMister);
  server.on("/display", handleDisplay);
  server.on("/sensors", handleSensors);
  server.on("/status", handleStatus);
  server.on("/prometheus", handlePrometheus);
  server.begin();

  if (!lcd->begin(16, 2))  {
    syslog.log(LOG_INFO, "LCD Initialization failed");
  }

  DHTSensors.setStorage(storage_array);
  setupPet(Mister, DHTSensors);
}

void loop() {
  static time_t prevTime = 0;
  static time_t now = 0;
  static bool misterRan = false;
  static int misterStatus = 0;
  static int prevMisterStatus = 0;
  static int avgHumidity = -1;
  ArduinoOTA.handle();
  server.handleClient();

  prevTime = now;
  now = time(nullptr);

  // check if there's a person and turn on the LCD backlight if there is
  motion->handle();
  if (motion->activity() && ! lcd->state) {
    lcd->setBackLight(true);
    syslog.log(LOG_INFO, "Person detected, turned backlight on");
  } 
  if (!motion->activity() && lcd->state) {
    lcd->setBackLight(false);
    syslog.log(LOG_INFO, "Nobody detected, turned backlight off");
  }

  // Gather data from sensors, only supported up to every 2s 
  int humiditySum = 0, sensorCount = 0;
  if ( now != prevTime && now % 2 == 0 ) {
    for (myDHT* sensor : DHTSensors) { 
      sensor->handle(); 
      // don't mist if above 60% humidity
      humiditySum = humiditySum + sensor->getHumidity();
      sensorCount++;
    }
    avgHumidity = humiditySum / sensorCount;
  }

  if (Mister->active && avgHumidity > Mister->moistureLevel) {
    Mister->setInActive();
    syslog.appName(MISTER_APPNAME);
    syslog.logf(LOG_INFO, "Setting mister INACTIVE.  Avg Humidity at %d, boundary at %d.", avgHumidity, Mister->moistureLevel);
    syslog.appName(SYSTEM_APPNAME);
  } 
  if (!Mister->active && avgHumidity < (Mister->moistureLevel - 1)) {
    Mister->setActive();
    syslog.appName(MISTER_APPNAME);
    syslog.logf(LOG_INFO, "Setting mister ACTIVE.  Humidity at %d, boundary at %d.", avgHumidity, Mister->moistureLevel);
    syslog.appName(SYSTEM_APPNAME);
  }

  // Handle mister API requests and status changes
  misterStatus = Mister->handle();

  // mister changed state
  if (prevMisterStatus != misterStatus) {
    syslog.appName(MISTER_APPNAME);
    if (Mister->on) {
      syslog.logf(LOG_INFO, "Scheduled mister run started, humidity at %d: ", avgHumidity);
    } else {
      syslog.logf(LOG_INFO, "Scheduled mister run finished, humidity at %d: ", avgHumidity);
    }
      syslog.appName(SYSTEM_APPNAME);
  }


  // display sensor data or mister data on lcd
  if ( now != prevTime ) {
    lcd->setCursor(0, 0);

    // If the mister is running, show the count down timer
    if ( Mister->status() ) {
      char row1[17];
      sprintf(row1, "Time left: %s", Mister->timeLeftToRun); 
      lcd->print(row1);
      misterRan = true;
    } else {
      //clear the screen if the mister just finished running
      if (misterRan) { 
	lcd->clear();
	misterRan=false;
      }

      // print the humdity on the 1st row
      // we use the cached sensor data from before
      lcd->setCursor(0, 0);
      lcd->print("H:");
      for (myDHT* sensor : DHTSensors) {
	char row1[10];
	sprintf(row1, " %4.2f%%", sensor->getHumidity());
	lcd->print(row1);
      }

      // print the temperature on the 2nd row
      lcd->setCursor(0, 1);
      lcd->print("T:");
      for (myDHT* sensor : DHTSensors) {
	char row2[10];
	sprintf(row2, " %4.2f", sensor->getTemp());
	lcd->print(row2);
      }
    }
  }
}
