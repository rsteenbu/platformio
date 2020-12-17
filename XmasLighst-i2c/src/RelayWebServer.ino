#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Syslog.h>
#include <ezTime.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_VEML7700.h>
//#include <Adafruit_AM2315.h>

#include <my_relay.h>
 
// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);
// Syslog server connection info
const char* SYSLOG_SERVER = "ardupi4";
const int SYSLOG_PORT = 514;
// This device info
const char* DEVICE_HOSTNAME = "iot-frontyard";
const char* APP_NAME = "xlmaslights";
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;
Timezone myTZ;

Relay xmasLights(D7);   
Relay Irrigation(D6);   
Relay lvLights(D5);   

// Adafruit temperature and humidity sensor
//Adafruit_AM2315 am2315;

// instantiate the display
Adafruit_SSD1306 display = Adafruit_SSD1306(128 /*width*/, 64 /*height*/, &Wire, -1);

// Adafruit i2c LUX sensor
Adafruit_VEML7700 veml = Adafruit_VEML7700();

void setup() {
  // start out with the relays off
  xmasLights.setup();
  Irrigation.setup();
  lvLights.setup();

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  syslog.logf(LOG_INFO, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());

  // Setup OTA Update
  ArduinoOTA.begin();

  // Start the HTTP server
  server.begin();

  //Set the time and timezone
  waitForSync();
  myTZ.setLocation(F("America/Los_Angeles"));
  myTZ.setDefault();

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

  if (!veml.begin()) {
    syslog.log(LOG_INFO, "VEML Sensor not found");
  }
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
  veml.interruptEnable(true);

  //Temp and Humidity sensor
//  if (! am2315.begin()) {
//     syslog.log(LOG_INFO, "Sensor not found, check wiring & pullups!");
//  }
}

int16_t lightLevel = 0;
int16_t soilMoistureLevel = 0;
float temperature = 0;
float humidity = 0;
int const dusk = 100;
int const morningHour = 6;
int const eveningHour = 23;
int const sampleSize = 10;
int timesDark = 0;
int timesLight = 0;
bool xmasLightOverride = false;
void loop() {
  ArduinoOTA.handle();

  // call the eztime events to update ntp date when it wants
  events();

  if (secondChanged()) {
    soilMoistureLevel = analogRead(A0);
    lightLevel = veml.readLux();
//    if (! am2315.readTemperatureAndHumidity(&temperature, &humidity)) {
//      syslog.logf(LOG_INFO, "Failed to read data from AM2315");
//    } 
    uint16_t irq = veml.interruptStatus();
    if (irq & VEML7700_INTERRUPT_LOW) {
      syslog.log(LOG_INFO, "** Low IRQ threshold"); 
    }
    if (irq & VEML7700_INTERRUPT_HIGH) {
      syslog.log(LOG_INFO, "** High IRQ threshold"); 
    }

    // print some stuff to the display
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(0);
    display.setTextColor(WHITE);
    display.printf("%02d/%02d/%d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
    display.println();
    display.printf("Light Level: %d", lightLevel);
    display.println();
    display.printf("Soil Moisture: %d", soilMoistureLevel);
    display.println();
    display.printf("Temperature: %f", temperature);
    display.println();
    display.printf("Humidty: %f", humidity);
    display.println();
    display.display();

    if (!xmasLightOverride) {
      if (!xmasLights.on) {
        // if it's dark and not between midnight and 6AM, turn the lights on
        if ((lightLevel < dusk) && ( hour() <= eveningHour && hour() > morningHour)) {
          timesDark++;
          if (debug) {
            syslog.logf(LOG_INFO, "DEBUG: light level below dusk %d for %d times", dusk, timesDark);
          }
          if (timesDark > sampleSize) {
            syslog.log(LOG_INFO, "Turning Xmas lights on");
            xmasLights.switchOn();
          }
        } else {
          timesDark = 0;
        } 
      } else {
        // if it's light or between midnight and 6AM, turn the lights off
        if ((lightLevel >= dusk) || ( hour() >= 0 && hour() <= 6)) {
          timesLight++;
          if (debug) {
            syslog.logf(LOG_INFO, "DEBUG: light level above dusk %d for %d times", dusk, timesLight);
          }
          if (timesLight > sampleSize) {
            syslog.log(LOG_INFO, "Turning Xmas lights off");
            xmasLights.switchOff();
          }
        } else {
          timesLight = 0;
        } 
      }
    } // xmasLightOverride
	}

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

  if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Turning debug off");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
  } else if (req.indexOf(F("/xmaslights/override/on")) != -1) {
    syslog.log(LOG_INFO, "Overriding Xmas light operation");
    xmasLightOverride = true;
    timesDark = 0;
    timesLight = 0;
  } else if (req.indexOf(F("/xmaslights/override/off")) != -1) {
    syslog.log(LOG_INFO, "Disabling Xmas light override");
    xmasLightOverride = false;
  } else if (req.indexOf(F("/xmaslights/override/status")) != -1) {
    client.print(xmasLightOverride);
  } else if (req.indexOf(F("/xmaslights/status")) != -1) {
    client.print(xmasLights.on);
  } else if (req.indexOf(F("/xmaslights/off")) != -1) {
    syslog.log(LOG_INFO, "Turning Xmas lights off");
    xmasLights.switchOff();
  } else if (req.indexOf(F("/xmaslights/on")) != -1) {
    syslog.log(LOG_INFO, "Turning Xmas lights on");
    xmasLights.switchOn();
  } else if (req.indexOf(F("/lvlights/status")) != -1) {
    client.print(lvLights.on);
  } else if (req.indexOf(F("/lvlights/off")) != -1) {
    syslog.log(LOG_INFO, "Turning lvlights lights off");
    lvLights.switchOff();
  } else if (req.indexOf(F("/lvlights/on")) != -1) {
    syslog.log(LOG_INFO, "Turning lvlights lights on");
    lvLights.switchOn();
  } else if (req.indexOf(F("/irrigation/status")) != -1) {
    client.print(Irrigation.on);
  } else if (req.indexOf(F("/irrigation/off")) != -1) {
    syslog.log(LOG_INFO, "Turning irrigation lights off");
    Irrigation.switchOff();
  } else if (req.indexOf(F("/irrigation/on")) != -1) {
    syslog.log(LOG_INFO, "Turning irrigation lights on");
    Irrigation.switchOn();
  } else if (req.indexOf(F("/status")) != -1) {
    client.printf("{\"switches\": {\"Xmas light\": {\"state\": \"%s\", \"override\": \"%s\"}, \"LV lights\": {\"state\": \"%s\"}, \"Irrigation\": {\"state\": \"%s\"}}, \"sensors\": {\"light\": %d, \"soil moisture\": %d, \"temperature\": %f, \"humidty\": %f}, \"debug\": %d}\n", 
				xmasLights.on ? "on" : "off", xmasLightOverride ? "on" : "off", lvLights.on ? "on" : "off", Irrigation.on ? "on" : "off", lightLevel, soilMoistureLevel, temperature, humidity, debug);
  } else {
    syslog.log(LOG_INFO, "received invalid request");
  }

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }
}

