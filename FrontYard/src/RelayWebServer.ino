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
#include <Adafruit_VEML7700.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <my_relay.h>

ESP8266WebServer server(80);

// This device info
#define MYTZ TZ_America_Los_Angeles
#define JSON_SIZE 450

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
Adafruit_VEML7700 veml = Adafruit_VEML7700();

int debug = 0;
char msg[40];

int const SOIL_PIN = A0;
int const PIR_PIN = D6;
int const ONEWIRE_PIN = D7;
// Create the relays
Relay xmasLights(D3);
Relay lvLights(D4);
TimerRelay irrigation(D5);

// MotionSensor setup
int pirState = LOW;  //start with no motion detected

// Thermometer Dallas OneWire
OneWire ds(ONEWIRE_PIN);  // on pin D7 (a 4.7K resistor is necessary)
DallasTemperature sensors(&ds);
DeviceAddress frontyardThermometer;

int16_t lightLevel = 0;
float temperature = 0;
bool displayOn = true;

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // prepare the motion sensor
  pinMode(PIR_PIN, INPUT);

  // Setup the Relays
  xmasLights.setup();
  lvLights.setup();
  irrigation.setup();
  irrigation.setEveryDayOn();
  irrigation.setSoilMoisture(SOIL_PIN);
  irrigation.setSoilMoisturePercentageToRun(86);
  irrigation.setRuntime(10);
  irrigation.setStartTime(8, 15);

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

  // Start the HTTP server
  server.on("/debug", handleDebug);
  server.on("/light", handleLight);
  server.on("/irrigation", handleIrrigation);
  server.on("/xmasLights", handleXmasLights);
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
  if (!veml.begin()) {
    syslog.log(LOG_INFO, "VEML Sensor not found");
  } else {
    veml.setGain(VEML7700_GAIN_1);
    veml.setIntegrationTime(VEML7700_IT_800MS);

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
    syslog.logf(LOG_INFO, "Turning light on at %ld", lvLights.onTime);
    lvLights.switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    syslog.logf(LOG_INFO, "Turning light off at %ld", lvLights.offTime);
    lvLights.switchOff();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", lvLights.on ? "1" : "0");
  } else if (server.arg("override") == "on") {
    syslog.log(LOG_INFO, "Disabling light schedule");
    lvLights.setScheduleOverride(true);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "off") {
    syslog.log(LOG_INFO, "Enabling light schedule");
    lvLights.setScheduleOverride(false);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "status") {
    server.send(200, "text/plain", lvLights.scheduleOverride ? "1" : "0");
  } else {
    server.send(404, "text/plain", "ERROR: unknown light command");
  }
}

void handleIrrigation() {
  if (server.arg("state") == "on") {
    syslog.logf(LOG_INFO, "Turning irrigation on at %ld", irrigation.onTime);
    irrigation.switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    syslog.logf(LOG_INFO, "Turning irrigation off at %ld", irrigation.offTime);
    irrigation.switchOff();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", irrigation.on ? "1" : "0");
  } else if (server.arg("override") == "on") {
    syslog.log(LOG_INFO, "Disabling irrigation schedule");
    irrigation.setScheduleOverride(true);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "off") {
    syslog.log(LOG_INFO, "Enabling irrigation schedule");
    irrigation.setScheduleOverride(false);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "status") {
    server.send(200, "text/plain", irrigation.scheduleOverride ? "1" : "0");
  } else {
    server.send(404, "text/plain", "ERROR: unknown irrigation command");
  }
}

void handleXmasLights() {
  if (server.arg("state") == "on") {
    syslog.logf(LOG_INFO, "Turning xmasLights on at %ld", xmasLights.onTime);
    xmasLights.switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    syslog.logf(LOG_INFO, "Turning xmasLights off at %ld", xmasLights.offTime);
    xmasLights.switchOff();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", xmasLights.on ? "1" : "0");
  } else if (server.arg("override") == "on") {
    syslog.log(LOG_INFO, "Disabling xmasLights schedule");
    xmasLights.setScheduleOverride(true);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "off") {
    syslog.log(LOG_INFO, "Enabling xmasLights schedule");
    xmasLights.setScheduleOverride(false);
    server.send(200, "text/plain");
  } else if (server.arg("override") == "status") {
    server.send(200, "text/plain", xmasLights.scheduleOverride ? "1" : "0");
  } else {
    server.send(404, "text/plain", "ERROR: unknown xmasLights command");
  }
}

void handleSensors() {
  if (server.arg("sensor") == "soilmoisture") {
    char msg[10];
    sprintf(msg, "%0.2f", irrigation.soilMoisturePercentage);
    server.send(200, "text/plain", msg);
  } else if (server.arg("sensor") == "light") {
    char msg[10];
    sprintf(msg, "%d", lightLevel);
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

  switches["irrigation"]["state"] = irrigation.state();
  switches["irrigation"]["override"] = irrigation.scheduleOverride;
  switches["lvLights"]["state"] = lvLights.state();
  switches["lvLights"]["override"] = lvLights.scheduleOverride;
  switches["xmasLights"]["state"] = xmasLights.state();
  switches["xmasLights"]["override"] = xmasLights.scheduleOverride;
  JsonObject sensors = doc.createNestedObject("sensors");
  sensors["temperature"] = temperature;
  sensors["lightLevel"] = lightLevel;
  sensors["soilMoisturePercentage"] = irrigation.soilMoisturePercentage;
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

int timesDark = 0;
int timesLight = 0;
time_t prevTime = 0;;
int prevIrrigationAction = 0;

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  time_t now = time(nullptr);
  if ( now != prevTime ) {
    if ( now % 5 == 0 ) {
      syslog.appName(IRRIGATION_APPNAME);
      int irrigationAction = irrigation.handle();
      if ( irrigationAction == 1 ) {
	syslog.log(LOG_INFO, "scheduled irrigation started");
      } 
      if ( irrigationAction == 2 ) {
	syslog.log(LOG_INFO, "scheduled irrigation stopped");
      }
      if ( prevIrrigationAction != 3 && irrigationAction == 3 ) {
	syslog.log(LOG_INFO, "scheduled irrigation not started because the soil's wet");
      }
      if ( prevIrrigationAction != 4 && irrigationAction == 4 ) {
	syslog.log(LOG_INFO, "scheduled irrigation stopped because the soil's wet");
      }

      prevIrrigationAction = irrigationAction;

      syslog.appName(SYSTEM_APPNAME);

      temperature = getTemperature();

      lightLevel = veml.readLux();

      updateDisplay(lightLevel, irrigation.soilMoisturePercentage, temperature);
      controlLightSchedule(lvLights, lightLevel);
    }
    prevTime = now;
  }
}

void updateDisplay(int16_t lightLevel, float soilMoisturePercentage, float temperature) {
    syslog.appName(MOTION_APPNAME);
    int pirVal = digitalRead(PIR_PIN);  // read input value from the motion sensor
    if (pirVal == HIGH) {            // check if the input is HIGH
      if (pirState == LOW) {
	// we have just turned on
	syslog.log(LOG_INFO, "Person detected, turning display on");
	display.ssd1306_command(SSD1306_DISPLAYON);
	displayOn = true;
      }
    } else {
      if (pirState == HIGH){
	syslog.log(LOG_INFO, "Nobody detected, turning display off");
	display.ssd1306_command(SSD1306_DISPLAYOFF);
	pirState = LOW;
	displayOn = false;
      }
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
      display.printf("Light Level: %d", lightLevel);
      display.println();
      display.printf("Soil Moisture: %0.2f", soilMoisturePercentage);
      display.println();
      display.printf("Temperature: %2.2f", temperature);
      display.println();
      display.display();
      pirState = HIGH;
   }
}

float getTemperature() {
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC = sensors.getTempC(frontyardThermometer);
  return DallasTemperature::toFahrenheit(tempC); // Converts tempC to Fahrenheit
}

void controlLightSchedule(Relay &lightSwitch, int16_t lightLevel) {
  int const dusk = 100;
  time_t now = time(nullptr);
  int const timesToSample = 10;
  int hour = localtime(&now)->tm_hour;
  int nightOffHour = 0;
  int morningOnHour = 5;

  syslog.appName(LIGHTSWITCH_APPNAME);
  if (lightSwitch.scheduleOverride) {
    return;
  }

  // Switch on criteria: it's dark, the lights are not on and it's not the middle of the night
  if ( lightLevel < dusk && !lightSwitch.on && !( nightOffHour >= 0 && hour <= morningOnHour ) ) {
    timesLight = 0;
    timesDark++;
    if (debug) {
      syslog.logf(LOG_INFO, "DEBUG: light level below dusk %d for %d times", dusk, timesDark);
    }
    if (timesDark > timesToSample) {
      syslog.log(LOG_INFO, "Turning lights on");
      lightSwitch.switchOn();
    }
  }

  // Switch off criteria: the lights are on and either it's light or it's in the middle of the night
  if (lightSwitch.on && (lightLevel > dusk || ( hour >= 0 && hour < 6 ))) {
    timesDark = 0;
    timesLight++;
    if (debug) {
      syslog.logf(LOG_INFO, "DEBUG: light level above dusk %d for %d times", dusk, timesLight);
    }
    if (timesLight > timesToSample) {
      syslog.log(LOG_INFO, "Turning lights off");
      lightSwitch.switchOff();
    }
  }
}
