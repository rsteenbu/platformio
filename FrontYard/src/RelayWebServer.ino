#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ezTime.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_VEML7700.h>

#include <OneWire.h>
#include <DallasTemperature.h>

OneWire ds(D7);  // on pin D7 (a 4.7K resistor is necessary)
DallasTemperature sensors(&ds);
DeviceAddress frontyardThermometer;

#include <my_relay.h>

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

// Syslog server connection info
#define SYSLOG_SERVER "ardupi4"
#define SYSLOG_PORT 514
// This device info
#define APP_NAME "switch"
#define JSON_SIZE 400

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;
// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);
// instantiate the display
Adafruit_SSD1306 display = Adafruit_SSD1306(128 /*width*/, 64 /*height*/, &Wire, -1);
// Adafruit i2c LUX sensor
Adafruit_VEML7700 veml = Adafruit_VEML7700();

int debug = 0;
Timezone myTZ;

char msg[40];
StaticJsonDocument<200> doc;

// MotionSensor setup
int const PIR_PIN = D6;
int pirState = LOW;  //start with no motion detected

// Create the relays
Relay irrigation(D5);
Relay lvLights(D3);
Relay xmasLights(D4);

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // prepare the motion sensor
  pinMode(PIR_PIN, INPUT);

  // Setup the Relay
  irrigation.setup();
  lvLights.setup();
  xmasLights.setup();

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

  //Set the time and timezone
  waitForSync();
  myTZ.setLocation(F("America/Los_Angeles"));
  myTZ.setDefault();

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
    veml.interruptEnable(true);
  }

  // Start up the DallasTemperature library
  sensors.begin();
  if (!sensors.getAddress(frontyardThermometer, 0)) { 
    syslog.log(LOG_INFO, "DS18B20: Unable to find address for Device 0"); 
  } else {
    sensors.setResolution(frontyardThermometer, 9);
  }
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
bool irrigationOverride = true;
bool lvLightsOverride = true;
bool xmasLightsOverride = true;
bool displayOn = true;

void loop() {

  ArduinoOTA.handle();

  if (secondChanged()) {

    temperature = getTemperature();
    soilMoistureLevel = analogRead(A0);
    lightLevel = veml.readLux();

    uint16_t irq = veml.interruptStatus();
    if (irq & VEML7700_INTERRUPT_LOW) {
      syslog.log(LOG_INFO, "** Low IRQ threshold"); 
    }
    if (irq & VEML7700_INTERRUPT_HIGH) {
      syslog.log(LOG_INFO, "** High IRQ threshold"); 
    }

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
	syslog.log(LOG_INFO, "Nobody detected, turning light off");
	display.ssd1306_command(SSD1306_DISPLAYOFF);
	pirState = LOW;
	displayOn = false;
      }
    }

    if (displayOn) {
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
      display.printf("Temperature: %2.2f", temperature);
      display.println();
      display.display();
      pirState = HIGH;
   }

    if (!xmasLightsOverride) {
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
    } // xmasLightsOverride
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
    
// Irrigation
  } else if (req.indexOf(F("/irrigation/status")) != -1) {
    client.print(irrigation.on);
  } else if (req.indexOf(F("/irrigation/override/off")) != -1) {
      syslog.log(LOG_INFO, "Enabling irrigation schedule");
      irrigationOverride = false;
  } else if (req.indexOf(F("/irrigation/override/on")) != -1) {
      syslog.log(LOG_INFO, "Enabling irrigation schedule");
      irrigationOverride = true;
  } else if (req.indexOf(F("/irrigation/off")) != -1) {
      syslog.log(LOG_INFO, "Turning irrigation off");
      irrigation.switchOff();
  } else if (req.indexOf(F("/irrigation/on")) != -1) {
      syslog.log(LOG_INFO, "Turning irrigation on");
      irrigation.switchOn();

// Low Voltage Lights
  } else if (req.indexOf(F("/lvLights/status")) != -1) {
    client.print(lvLights.on);
  } else if (req.indexOf(F("/lvLights/override/off")) != -1) {
      syslog.log(LOG_INFO, "Disabling lvLights schedule");
      lvLightsOverride = false;
  } else if (req.indexOf(F("/lvLights/override/on")) != -1) {
      syslog.log(LOG_INFO, "Enabling lvLights schedule");
      lvLightsOverride = true;
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
      syslog.log(LOG_INFO, "Disabling xmasLights schedule");
      xmasLightsOverride = false;
  } else if (req.indexOf(F("/xmasLights/override/on")) != -1) {
      syslog.log(LOG_INFO, "Enabling xmasLights schedule");
      xmasLightsOverride = true;
  } else if (req.indexOf(F("/xmasLights/off")) != -1) {
      syslog.log(LOG_INFO, "Turning xmasLights off");
      xmasLights.switchOff();
  } else if (req.indexOf(F("/xmasLights/on")) != -1) {
      syslog.log(LOG_INFO, "Turning xmasLights on");
      xmasLights.switchOn();

  } else if (req.indexOf(F("/status")) != -1) {
    StaticJsonDocument<JSON_SIZE> doc;
    JsonObject switches = doc.createNestedObject("switches");
    switches["irrigation"]["state"] = irrigation.state();
    switches["irrigation"]["override"] = irrigationOverride;
    switches["lvLights"]["state"] = lvLights.state();
    switches["lvLights"]["override"] = lvLightsOverride;
    switches["xmasLights"]["state"] = xmasLights.state();
    switches["xmasLights"]["override"] = xmasLightsOverride;
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["temperature"] = temperature;
    sensors["lightLevel"] = lightLevel;
    sensors["soilMoistureLevel"] = soilMoistureLevel;
    doc["debug"] = debug;
    doc["displayOn"] = displayOn;

    size_t jsonDocSize = measureJsonPretty(doc);
    if (jsonDocSize > JSON_SIZE) {
      client.printf("ERROR: JSON message too long, %d", jsonDocSize);
      client.println();
//      syslog.log(LOG_INFO, "JSON message too long");
    } else {
      serializeJsonPretty(doc, client);
      client.println();
    }
  } else {
    syslog.log("received invalid request");
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

float getTemperature() {
  sensors.requestTemperatures(); // Send the command to get temperatures
  float tempC = sensors.getTempC(frontyardThermometer);
  return DallasTemperature::toFahrenheit(tempC); // Converts tempC to Fahrenheit
}

