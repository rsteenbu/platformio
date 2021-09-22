#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <my_relay.h>
#include <my_pir.h>

ESP8266WebServer server(80);

// This device info
#define MYTZ TZ_America_Los_Angeles
#define JSON_SIZE 1000

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;
// Create a new syslog instance with LOG_LOCAL0 facility
#define SYSTEM_APPNAME "system"
#define LIGHT_APPNAME "lightsensor"
#define THERMO_APPNAME "thermometer"
#define MOTION_APPNAME "motionsensor"
#define LIGHTSWITCH_APPNAME "lightswitch"
#define IRRIGATION_APPNAME "irrigation"
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, SYSTEM_APPNAME, LOG_LOCAL0);
// instantiate the display
Adafruit_SSD1306 display = Adafruit_SSD1306(128 /*width*/, 64 /*height*/, &Wire, -1);
// Adafruit i2c LUX sensor

int debug = 0;
char msg[40];

int const SOIL_PIN = A0;
int const ONEWIRE_PIN = D7;

// Thermometer Dallas OneWire
OneWire ds(ONEWIRE_PIN);  // on pin D7 (a 4.7K resistor is necessary)
DallasTemperature sensors(&ds);
DeviceAddress frontyardThermometer;

DuskToDawnScheduleRelay * lvLights = new DuskToDawnScheduleRelay(D4);
IrrigationRelay * irrigation = new IrrigationRelay(D5);

PIR * motionsensor = new PIR(D6);

float temperature = 0;
bool displayOn = true;

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // prepare the motion sensor
  motionsensor->setup();

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
 
  // set the time
  configTime(MYTZ, "pool.ntp.org");

  // Dusk To Dawn LV Lights
  lvLights->setup("lvlights");
  lvLights->setNightOffOnHours(0,5);
  if (! lvLights->setVemlLightSensor()) {
    syslog.log(LOG_INFO, "ERROR: Setup of Veml Sensor failed");
  }

  // Frontyard Irrigation
  irrigation->setup("frontyard");
  irrigation->setRuntime(10*60);
  irrigation->setStartTime(8, 15);
  irrigation->setSoilMoistureSensor(SOIL_PIN, 86); // pin for analog read, percentage to run at
  irrigation->setSoilMoistureLimits(660, 330); //dry level, wet level

  // Start the HTTP server
  server.on("/debug", handleDebug);
  server.on("/light", handleLight);
  server.on("/irrigation", handleIrrigation);
  server.on("/status", handleStatus);
  server.on("/sensors", handleSensors);
  server.begin();

  // set I2C pins (SDA, SDL)
  Wire.begin(D2, D1);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    syslog.log(LOG_INFO, "SSD1306 allocation failed");
  }
  
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  syslog.appName(LIGHT_APPNAME);

  // Start up the DallasTemperature library
  syslog.appName(THERMO_APPNAME);
  sensors.begin();
  if (!sensors.getAddress(frontyardThermometer, 0)) { 
    syslog.log(LOG_INFO, "DS18B20: Unable to find address for Device 0"); 
  } else {
    sensors.setResolution(frontyardThermometer, 9);
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
    syslog.logf(LOG_INFO, "Turned %s on by API request", lvLights->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    lvLights->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off by API request", lvLights->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", lvLights->on ? "1" : "0");
  } else if (server.arg("override") == "on") {
    syslog.log(LOG_INFO, "Disabling light schedule");
    lvLights->setScheduleOverride(true);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "off") {
    syslog.log(LOG_INFO, "Enabling light schedule");
    lvLights->setScheduleOverride(false);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "status") {
    server.send(200, "text/plain", lvLights->scheduleOverride ? "1" : "0");
  } else {
    server.send(404, "text/plain", "ERROR: unknown light command");
  }
}

void handleIrrigation() {
  if (server.arg("state") == "on") {
    syslog.logf(LOG_INFO, "Turned irrigation %s on for %ds", irrigation->name, irrigation->runTime);
    irrigation->switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    irrigation->switchOff();
    syslog.logf(LOG_INFO, "Turned irrigation %s off", irrigation->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", irrigation->status() ? "1" : "0");
  } else if (server.arg("override") == "on") {
    syslog.log(LOG_INFO, "Disabling irrigation schedule");
    irrigation->setScheduleOverride(true);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "off") {
    syslog.log(LOG_INFO, "Enabling irrigation schedule");
    irrigation->setScheduleOverride(false);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "status") {
    server.send(200, "text/plain", irrigation->scheduleOverride ? "1" : "0");
  } else {
    server.send(404, "text/plain", "ERROR: unknown irrigation command");
  }
}

void handleSensors() {
  if (server.arg("sensor") == "soilmoisture") {
    char msg[10];
    sprintf(msg, "%0.2f", irrigation->soilMoisturePercentage);
    server.send(200, "text/plain", msg);
  } else if (server.arg("sensor") == "light") {
    char msg[10];
    sprintf(msg, "%d", lvLights->lightLevel);
    server.send(200, "text/plain", msg);
  } else if (server.arg("sensor") == "temperature") {
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
  switches["irrigation"]["state"] = irrigation->state();
  switches["irrigation"]["override"] = irrigation->scheduleOverride;
  switches["irrigation"]["Time Left"] = irrigation->timeLeftToRun;
  switches["irrigation"]["Last Run Time"] = irrigation->prettyOnTime;
  switches["irrigation"]["Next Run Time"] = irrigation->nextTimeToRun;
  switches["lvLights"]["state"] = lvLights->state();
  switches["lvLights"]["override"] = lvLights->scheduleOverride;

  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["temperature"] = temperature;
  sensors["lightLevel"] = lvLights->lightLevel;
  sensors["soilMoistureLevel"] = irrigation->soilMoistureLevel;
  sensors["soilMoisturePercentage"] = irrigation->soilMoisturePercentage;
  char timeString[20];
  struct tm *timeinfo = localtime(&now);
  strftime (timeString,20,"%D %T",timeinfo);
  doc["time"] = timeString;
  doc["debug"] = debug;
  doc["displayOn"] = displayOn;

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

time_t prevTime = 0;
time_t now = 0;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  syslog.appName(IRRIGATION_APPNAME);
  if ( now != prevTime && debug >= 2 )  {
    syslog.log(LOG_INFO, "DEBUG: Enabled");

    String timesToStart = "DEBUG: times to start: ";
    irrigation->checkStartTime(timesToStart);
    syslog.log(LOG_INFO, timesToStart);
    
    if ( irrigation->checkDayToRun() ) {
      syslog.log(LOG_INFO, "DEBUG: Enabled for today");
    }
    if ( irrigation->isTimeToStart() ) {
      syslog.log(LOG_INFO, "DEBUG: it's time to start");
    }
  }

  if ( now != prevTime )  {
    String reason;
    if ( irrigation->handle() ) {
      syslog.logf(LOG_INFO, "%s %s; soil moisture: %f%%", irrigation->name, irrigation->state(), irrigation->soilMoisturePercentage);
    } 
  }

  syslog.appName(LIGHTSWITCH_APPNAME);
  if (lvLights->handle()) {
    syslog.logf(LOG_INFO, "Turned %s %s", lvLights->name, lvLights->state());
  }

  syslog.appName(SYSTEM_APPNAME);
  prevTime = now;
  now = time(nullptr);
  if ( ( now != prevTime ) && ( now % 5 == 0 ) ) {
     temperature = getTemperature();
     motionsensor->handle();
     updateDisplay();
  }
}

void updateDisplay() {
    syslog.appName(MOTION_APPNAME);
    if (motionsensor->activity() && !displayOn) {            // check if the input is HIGH
      display.ssd1306_command(SSD1306_DISPLAYON);
      displayOn = true;
      syslog.log(LOG_INFO, "Person detected, turned display on");
    }
    if (!motionsensor->activity() && displayOn) {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      displayOn = false;
      syslog.log(LOG_INFO, "Nobody detected, turned display off");
    }

    if (displayOn) {
      // print some stuff to the display
      char timeString[20];
      time_t now = time(nullptr);
      struct tm *timeinfo = localtime(&now);
      strftime (timeString,20,"%D %T",timeinfo);

      display.clearDisplay();
      display.setCursor(0,0);
      display.setTextSize(0);
      display.setTextColor(WHITE);
      display.println(timeString);
      display.printf("Light Level: %d", lvLights->lightLevel);
      display.println();
      display.printf("Soil Moisture: %0.2f", irrigation->soilMoisturePercentage);
      display.println();
      display.printf("Temperature: %2.2f", temperature);
      display.println();
      display.display();
   }
}

float getTemperature() {
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC = sensors.getTempC(frontyardThermometer);
  return DallasTemperature::toFahrenheit(tempC); // Converts tempC to Fahrenheit
}

