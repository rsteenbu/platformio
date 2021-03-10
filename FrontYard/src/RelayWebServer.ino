#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
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

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

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

int drySoilMoistureLevel = 387;
int wet = 500;

int debug = 0;
char msg[40];

int const SOIL_PIN = A0;
int const PIR_PIN = D6;
int const ONEWIRE_PIN = D7;
// Create the relays
Relay xmasLights(D3);
Relay lvLights(D4);
Relay irrigation(D5);

// MotionSensor setup
int pirState = LOW;  //start with no motion detected

// Thermometer Dallas OneWire
OneWire ds(ONEWIRE_PIN);  // on pin D7 (a 4.7K resistor is necessary)
DallasTemperature sensors(&ds);
DeviceAddress frontyardThermometer;

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // prepare the motion sensor
  pinMode(PIR_PIN, INPUT);

  // Setup the Relay
  irrigation.setup();
  xmasLights.setup();
  lvLights.setup();

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

  // Start the server
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
}

static time_t now;
int16_t lightLevel = 0;
int16_t soilMoistureLevel = 0;
float temperature = 0;
bool displayOn = true;
int timesDark = 0;
int timesLight = 0;
int prevSeconds = 0;;

void loop() {

  ArduinoOTA.handle();
  now = time(nullptr);

  int currSeconds = localtime(&now)->tm_sec;
  if ( currSeconds != prevSeconds ) {

    temperature = getTemperature();
    soilMoistureLevel = analogRead(SOIL_PIN);
    lightLevel = veml.readLux();

    updateDisplay(lightLevel, soilMoistureLevel, temperature);

    controlLightSchedule(lvLights, lightLevel);
  }
  prevSeconds = currSeconds;

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // setup a new client
  client.setTimeout(5000); // default is 1000

  // Read the first line of the request
  String req = client.readStringUntil('\r');

  // Tell the client we're ok
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  syslog.appName(SYSTEM_APPNAME);
  if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Turning debug off");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
    
// Irrigation
  } else if (req.indexOf(F("/irrigation/status")) != -1) {
    client.print(irrigation.on);
  } else if (req.indexOf(F("/irrigation/override/off")) != -1) {
      syslog.log(LOG_INFO, "Enabling irrigation schedule");
      irrigation.setScheduleOverride(false);
  } else if (req.indexOf(F("/irrigation/override/on")) != -1) {
      syslog.log(LOG_INFO, "Disabling irrigation schedule");
      irrigation.setScheduleOverride(true);
  } else if (req.indexOf(F("/irrigation/off")) != -1) {
      syslog.log(LOG_INFO, "Turning irrigation off");
      irrigation.switchOff();
  } else if (req.indexOf(F("/irrigation/override/status")) != -1) {
      client.print(irrigation.getScheduleOverride());
  } else if (req.indexOf(F("/irrigation/on")) != -1) {
      syslog.log(LOG_INFO, "Turning irrigation on");
      irrigation.switchOn();

// Low Voltage Lights
  } else if (req.indexOf(F("/lvLights/status")) != -1) {
    client.print(lvLights.on);
  } else if (req.indexOf(F("/lvLights/override/off")) != -1) {
      syslog.log(LOG_INFO, "Enabling lvLights schedule");
      lvLights.setScheduleOverride(false);
  } else if (req.indexOf(F("/lvLights/override/on")) != -1) {
      syslog.log(LOG_INFO, "Disabling lvLights schedule");
      lvLights.setScheduleOverride(true);
  } else if (req.indexOf(F("/lvLights/override/status")) != -1) {
      client.print(lvLights.getScheduleOverride());
  } else if (req.indexOf(F("/lvLights/off")) != -1) {
      syslog.log(LOG_INFO, "Turning lvLights off");
      lvLights.switchOff();
  } else if (req.indexOf(F("/lvLights/on")) != -1) {
      syslog.log(LOG_INFO, "Turning lvLights on");
      lvLights.switchOn();

// xMasLights
  } else if (req.indexOf(F("/xmasLights/status")) != -1) {
    client.print(xmasLights.on);
  } else if (req.indexOf(F("/xmasLights/override/off")) != -1) {
    syslog.log(LOG_INFO, "Enabling xmasLights schedule");
    xmasLights.setScheduleOverride(false);
  } else if (req.indexOf(F("/xmasLights/override/on")) != -1) {
    syslog.log(LOG_INFO, "Disabling xmasLights schedule");
    xmasLights.setScheduleOverride(true);
  } else if (req.indexOf(F("/xmasLights/override/status")) != -1) {
      client.print(xmasLights.getScheduleOverride());
  } else if (req.indexOf(F("/xmasLights/off")) != -1) {
    syslog.log(LOG_INFO, "Turning xmasLights off");
    xmasLights.switchOff();
  } else if (req.indexOf(F("/xmasLights/on")) != -1) {
    syslog.log(LOG_INFO, "Turning xmasLights on");
    xmasLights.switchOn();

 // Sensors
  } else if (req.indexOf(F("/sensors/soilMoisture")) != -1) {
    client.print(soilMoistureLevel);
  } else if (req.indexOf(F("/sensors/light")) != -1) {
    client.print(lightLevel);
  } else if (req.indexOf(F("/sensors/temperatur")) != -1) {
    client.print(temperature);

  } else if (req.indexOf(F("/status")) != -1) {
    StaticJsonDocument<JSON_SIZE> doc;
    JsonObject switches = doc.createNestedObject("switches");
    switches["irrigation"]["state"] = irrigation.state();
    switches["irrigation"]["override"] = irrigation.getScheduleOverride();
    switches["lvLights"]["state"] = lvLights.state();
    switches["lvLights"]["override"] = lvLights.getScheduleOverride();
    switches["xmasLights"]["state"] = xmasLights.state();
    switches["xmasLights"]["override"] = xmasLights.getScheduleOverride();
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["temperature"] = temperature;
    sensors["lightLevel"] = lightLevel;
    sensors["soilMoistureLevel"] = soilMoistureLevel;
    char timeString[20];
    struct tm *timeinfo = localtime(&now);
    strftime (timeString,20,"%D %T",timeinfo);
    doc["time"] = timeString;
    doc["debug"] = debug;
    doc["displayOn"] = displayOn;

    size_t jsonDocSize = measureJsonPretty(doc);
    if (jsonDocSize > JSON_SIZE) {
      client.printf("ERROR: JSON message too long, %d", jsonDocSize);
      client.println();
    } else {
      serializeJsonPretty(doc, client);
      client.println();
    }
  } else {
    syslog.log(LOG_INFO, "received invalid request");
  }

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  // The client will actually be *flushed* then disconnected
  // when the function returns and 'client' object is destroyed (out-of-scope)
  // flush = ensure written data are received by the other side
}

void updateDisplay(int16_t lightLevel, int16_t soilMoistureLevel, float temperature) {
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
      struct tm *timeinfo = localtime(&now);
      strftime (timeString,20,"%D %T",timeinfo);

      display.clearDisplay();
      display.setCursor(0,0);
      display.setTextSize(0);
      display.setTextColor(WHITE);
      display.println(timeString);
      display.printf("Light Level: %d", lightLevel);
      display.println();
      display.printf("Soil Moisture: %d", soilMoistureLevel);
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
  int const timesToSample = 10;
  int hour = localtime(&now)->tm_hour;

  syslog.appName(LIGHTSWITCH_APPNAME);
  if (lightSwitch.getScheduleOverride()) {
    return;
  }

    // if it's dark, the lights are not on and it's not the middle of the night when no one cares
  if ( lightLevel < dusk && !lightSwitch.on && !( hour >= 0 && hour < 6 ) ) {
    timesLight = 0;
    timesDark++;
    if (debug) {
      syslog.logf(LOG_INFO, "DEBUG: light level below dusk %d for %d times", dusk, timesDark);
    }
    if (timesDark > timesToSample) {
      syslog.log(LOG_INFO, "Turning lights on");
      lightSwitch.switchOn();
    }
    // if the lights are on and it's light or in the middle of the night, turn them off
  } else if (lightSwitch.on && (lightLevel > dusk || ( hour >= 0 && hour < 6 ))) {
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


void controlIrrigationSchedule(Relay &irrigationSwitch, int16_t soilMoistureLevel) {
  syslog.appName(IRRIGATION_APPNAME);
   if (irrigationSwitch.getScheduleOverride()) {
     return;
   }

  if (soilMoistureLevel >= wet) {
    if (irrigationSwitch.on) {
      syslog.log(LOG_INFO, "Turning irrigation off because the soil's wet");
      irrigationSwitch.switchOff();
    }
    return;
  }

  // 3 times / week for 15 minutes
} 
