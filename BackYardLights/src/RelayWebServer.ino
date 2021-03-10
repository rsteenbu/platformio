#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_VEML7700.h>
#include <OneWire.h>

#include <Adafruit_AM2315.h>
Adafruit_AM2315 am2315;

#include <my_relay.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#define MYTZ TZ_America_Los_Angeles

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

#define JSON_SIZE 300
#define SYSTEM_APPNAME "system"
#define LIGHT_APPNAME "lightsensor"
#define THERMO_APPNAME "thermometer"
#define LIGHTSWITCH_APPNAME "lightswitch"

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, SYSTEM_APPNAME, LOG_LOCAL0);

int debug = 0;

// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;

char msg[40];
StaticJsonDocument<200> doc;
Relay lvLights(TX_PIN);

Adafruit_VEML7700 veml = Adafruit_VEML7700();

bool vemlSensorFound = false;
bool thermoSensorFound = false;
void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // Setup the Relay
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

  configTime(MYTZ, "pool.ntp.org");

  /* 
  //Set the time and timezone
  waitForSync();
  myTZ.setLocation(F("America/Los_Angeles"));
  myTZ.setDefault();
  */

  // Start the server
  server.begin();

  // set I2C pins (SDA, SDL)
  Wire.begin(GPIO2_PIN, GPIO0_PIN);

  syslog.appName(LIGHT_APPNAME);
  int attempts = 0; 
  while (attempts < 10) {
    if (veml.begin()) {
      vemlSensorFound = true;
      syslog.log(LOG_INFO, "VEML Sensor found");
      break;
    }
    syslog.logf(LOG_INFO, "VEML Sensor not found, try %d", attempts);
    attempts++; 
    delay(1000);
  } 

  if (!vemlSensorFound) {
    syslog.logf(LOG_INFO, "VEML Sensor not found, giving up");
  } else {
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

  syslog.appName(THERMO_APPNAME);
  attempts = 0;
  while (attempts < 10) {
    if (am2315.begin()) {
      thermoSensorFound = true;
      syslog.log(LOG_INFO, "AMS23315 Sensor found!");
      break;
    }
    syslog.logf(LOG_INFO, "AMS23315 Sensor not found, try %d", attempts);
    attempts++; 
    delay(1000);
  } 

  if (!thermoSensorFound) {
    syslog.logf(LOG_INFO, "AMS23315 Sensor not found, giving up");
  } 
}

static time_t now;
int16_t lightLevel = 0;
int prevSeconds = 0;;
float temperature, humidity = -1;
void loop() {
  ArduinoOTA.handle();
  //gettimeofday(&tv, nullptr);
  //clock_gettime(0, &tp);
  now = time(nullptr);
    
  int currSeconds = localtime(&now)->tm_sec;
  if ( currSeconds != prevSeconds ) {
    if ( debug ) {
      syslog.logf(LOG_INFO, "seconds changed, now: %d, prev: %d", currSeconds, prevSeconds);
    }
    if ( currSeconds % 5 == 0 ) {
      if ( vemlSensorFound ) {
	lightLevel = veml.readLux();
      }
      syslog.appName(THERMO_APPNAME);
      if ( thermoSensorFound ) {
	if (! am2315.readTemperatureAndHumidity(&temperature, &humidity)) {
	  syslog.log(LOG_INFO, "Failed to read data from AM2315");
	}
      }
      // convert the temperature to F
      temperature = ((temperature * 9) / 5 + 32);
    }
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

  if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Turning debug off");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
  } else if (req.indexOf(F("/light/status")) != -1) {
    client.print(lvLights.on);
  } else if (req.indexOf(F("/light/off")) != -1) {
      syslog.log(LOG_INFO, "Turning light off");
      lvLights.switchOff();
  } else if (req.indexOf(F("/light/on")) != -1) {
      syslog.log(LOG_INFO, "Turning light on");
      lvLights.switchOn();
  } else if (req.indexOf(F("/status")) != -1) {
    StaticJsonDocument<JSON_SIZE> doc;
    JsonObject switches = doc.createNestedObject("switches");
    switches["light"]["state"] = lvLights.state();
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["lightLevel"] = lightLevel;
    sensors["temperature"] = temperature;
    sensors["humidity"] = humidity;
    doc["debug"] = debug;
    doc["ctime"] = ctime(&now);
    doc["seconds"] = localtime(&now)->tm_sec;

    if (measureJsonPretty(doc) > JSON_SIZE) {
      client.println("ERROR: JSON message too long");
      syslog.log(LOG_INFO, "JSON message too long");
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
