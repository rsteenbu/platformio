#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Syslog.h>
#include <ezTime.h>

#define ADALIB 1

const int RST_PIN  = D1; //white
const int DC_PIN   = D6; //green    Hardware SPI MISO = GPIO12 (not used)
const int SCLK_PIN = D5; //yellow   Hardware SPI CLK  = GPIO14
const int MOSI_PIN = D7; //blue     Hardware SPI MOSI = GPIO13
const int CS_PIN   = D8; //orange   Hardware SPI /CS  = GPIO15 (not used), pull-down 10k to GND

#if ADALIB == 1
#include <Wire.h>
#include <Adafruit_SSD1351.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
const int SCREEN_WIDTH = 128; // OLED display width, in pixels
const int SCREEN_HEIGHT = 128; // OLED display height, in pixels
Adafruit_SSD1351 display = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, CS_PIN, DC_PIN, MOSI_PIN, SCLK_PIN, RST_PIN);  
//Adafruit_SSD1351 display = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, CS_PIN, DC_PIN, RST_PIN);
#else
#include <SSD_13XX.h>
SSD_13XX display = SSD_13XX(CS_PIN, DC_PIN);
#endif

// Create an instance of the server
// specify the port to listen on as an argument
const char* ssid = "***REMOVED***";
const char* password = "***REMOVED***";
WiFiServer server(80);

// Syslog server connection info
const char* SYSLOG_SERVER = "ardupi4";
const int SYSLOG_PORT = 514;
// This device info
const char* DEVICE_HOSTNAME = "iot-computerroom";
const char* APP_NAME = "lightswitch";
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;

// Pin for Relay
int const RELAY_PIN = D3;

// distance sensory Pins; vin == black grnd == white
int const ECHO_PIN = D2;  //yellow
int const TRIG_PIN = D4;  //green

// Distance Sensor
float duration, distance;
float aggregattedDistance = 0;
float const distanceToMe = 40.0;
int const iterationsBeforeOff = 100;
int const sampleSize = 20;
bool sensorActive = true;
bool lightOn = false;

struct displayValues {
  int uptimeDays = -1;
  int uptimeHours = -1;
  int uptimeMinutes = -1;
  int uptimeSeconds = -1;
  int lightLevel;
  float averageDistance;
  String date;
  String time;
  int timeSeconds;
};

struct displayValues prevValues;
struct displayValues currValues;
int secondsUptime;

int iteration = 0;
int timesEmpty = 0;

Timezone myTZ;

// Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0  
#define WHITE           0xFFFF


void controlLight() {
  // Check the sensor to see if I'm at my desk

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);
  distance = (duration/2)/74.052;
  if (debug == 2) {
    syslog.logf("Iteration: %d; Distance: %f", iteration, distance);
  }

  // Work with an average over a bunch of samples
  aggregattedDistance = aggregattedDistance + distance;
  if (iteration % sampleSize == 0) {
    currValues.averageDistance = aggregattedDistance / sampleSize;
    if (debug) {
      syslog.logf("Average distance: %f\"", currValues.averageDistance);
    }
    aggregattedDistance = 0;

    if ( !lightOn && currValues.averageDistance < distanceToMe) {
      // I'm at my desk, turn the light on
      syslog.log(LOG_INFO, "Person detected, turning light on");
      digitalWrite(RELAY_PIN, LOW);
      timesEmpty = 0;
      lightOn = true;
    } else {
      // if the lights on, let's make sure I'm really not at my desk
      if (currValues.averageDistance > distanceToMe) {
        timesEmpty++;
      } else {
        timesEmpty = 0;
      }
    }

    // If I've been away for a while, turn the light off
    if (lightOn && timesEmpty > iterationsBeforeOff) {
      syslog.log(LOG_INFO, "Nobody detected, Turning light off");
      digitalWrite(RELAY_PIN, HIGH);
      lightOn = false;
      // Set the variables back to initial values
      aggregattedDistance = 0;
      timesEmpty = 0;
    }
  }
  delay(10);
}

void setup() {
  // prepare HW SPI Pins
  pinMode(SCLK_PIN, OUTPUT);
  pinMode(MOSI_PIN, OUTPUT);


  // prepare LED and Relay PINs
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // prepare the Distance Sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();
  Serial.println(F("WiFi connected"));

  syslog.logf(LOG_INFO, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());

  // Setup OTA Update
  ArduinoOTA.begin();

  // Start the server
  server.begin();
  Serial.println(F("Server started"));

  // Print the IP address
  Serial.println(WiFi.localIP());

  //Set the time
  waitForSync();
  myTZ.setLocation(F("America/Los_Angeles"));

  //initialize the display
#if ADALIB == 1
  display.begin();
  display.fillScreen(BLACK);
  display.fillRect(70, 0, 52, 8, RED);
  display.fillRect(70, 9, 52, 8, RED);
  display.setCursor(0,0);
  display.print("light level");
  display.setCursor(0,9);
  display.print("distance");
  display.setCursor(0,18);
  display.print("uptime:");
#else
  display.begin();
  display.print("Hello World!");
#endif

}

void loop() {
  ArduinoOTA.handle();

  // call the eztime events to update ntp date when it wants
  events();

  // make sure we don't overflow iteration
  if (iteration == 100000) {
    iteration = 0;
  }

  iteration++;

  if (secondChanged()) {
    secondsUptime = (int) millis() / 1000;
    currValues.uptimeDays    = secondsUptime / (60*60*24);
    currValues.uptimeHours   = (secondsUptime % (60*60*24)) / (60*60);
    currValues.uptimeMinutes = ((secondsUptime % (60*60*24)) % (60*60)) / 60;
    currValues.uptimeSeconds = secondsUptime % 60;
    // airValue = 785
    // waterValue = 470
    // soilmoisturepercent = map(soilMoistureValue, airValue, waterValue, 0, 100);
    currValues.lightLevel = analogRead(A0); 
    currValues.date = myTZ.dateTime("m/d/y");
    currValues.time = myTZ.dateTime("H:i");
    currValues.timeSeconds = (int) myTZ.second();

#if ADALIB == 1
    display.setTextSize(0);

    display.setTextColor(BLACK, RED);
    display.setCursor(75,0);
    display.print(prevValues.lightLevel);
    display.setCursor(75,9);
    display.print(prevValues.averageDistance);

    display.setTextColor(WHITE, RED);
    display.setCursor(75,0);
    display.print(currValues.lightLevel);
    display.setCursor(75,9);
    display.print(currValues.averageDistance);


    if (currValues.uptimeDays != prevValues.uptimeDays) {
      display.setCursor(56,18);
      display.printf("%02d", prevValues.uptimeDays);
      display.setCursor(56,18);
      display.printf("%02d",currValues.uptimeDays);
    }
    if (currValues.uptimeHours != prevValues.uptimeHours) {
      display.setCursor(68,18);
      display.printf(":%02d", prevValues.uptimeHours);
      display.setCursor(68,18);
      display.printf(":%02d",currValues.uptimeHours);
    }
    if (currValues.uptimeMinutes != prevValues.uptimeMinutes) {
      display.setCursor(86,18);
      display.printf(":%02d", prevValues.uptimeMinutes);
      display.setCursor(86,18);
      display.printf(":%02d",currValues.uptimeMinutes);
    }
    if (currValues.uptimeSeconds != prevValues.uptimeSeconds) {
      display.setCursor(104,18);
      display.printf(":%02d", prevValues.uptimeSeconds);
      display.setCursor(104,18);
      display.printf(":%02d",currValues.uptimeSeconds);
    }

    //print the date
    if (currValues.date != prevValues.date) {
      display.setCursor(0,29);
      display.setTextColor(BLACK);
      display.print(prevValues.date);
      display.setCursor(0,29);
      display.setTextColor(WHITE);
      display.print(currValues.date);
    }
    
    //print the hour and minute
    if (currValues.time != prevValues.time) {
      display.setCursor(60,29);
      display.setTextColor(BLACK);
      display.print(prevValues.time);
      display.setCursor(60,29);
      display.setTextColor(WHITE);
      display.print(currValues.time);
    } 
    
    //print the second
    display.setCursor(88,29);
    display.setTextColor(BLACK);
    display.printf(":%02d", prevValues.timeSeconds);
    display.setCursor(88,29);
    display.setTextColor(WHITE);
    display.printf(":%02d", currValues.timeSeconds);

    prevValues = currValues;

#endif

  }

  if (sensorActive) {
    controlLight();
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
  
  delayMicroseconds(50);

  // Tell the client we're ok
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  // Check the request and determine what to do with the switch relay
  if (req.indexOf(F("/light/on")) != -1) {
    if (lightOn) {
      syslog.log(LOG_INFO, "On requested, but light is already on");
    } else {
      syslog.log(LOG_INFO, "Turning light on as requested");
      digitalWrite(RELAY_PIN, LOW);
      lightOn = true;
    }
  } else if (req.indexOf(F("/light/off")) != -1) {
    if (lightOn) {
      syslog.log(LOG_INFO, "Turning light off as requested");
      digitalWrite(RELAY_PIN, HIGH);
      lightOn = false;
    } else {
      syslog.log(LOG_INFO, "Off requested, but light is already off");
    }
  } else if (req.indexOf(F("/light/status")) != -1) {
    if (debug == 2) {
      syslog.logf(LOG_INFO, "Light status: %s", lightOn ? "on" : "off");
    }
    client.print(lightOn);
  } else if (req.indexOf(F("/sensor/on")) != -1) {
    syslog.log(LOG_INFO, "Turning motion sensor switiching on");
    sensorActive = true;
  } else if (req.indexOf(F("/sensor/off")) != -1) {
    syslog.log(LOG_INFO, "Turning motion sensor switiching off");
    sensorActive = false;
    // Set the variables back to initial values
    iteration = 0;
    aggregattedDistance = 0;
    timesEmpty = 0;
  } else if (req.indexOf(F("/sensor/status")) != -1) {
    if (debug == 2) {
      syslog.logf(LOG_INFO, "Distance Sensor switching status: %s", sensorActive ? "on" : "off");
    }
    client.print(sensorActive);
  } else if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Setting debug level: 0");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Setting debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Setting debug level 2");
    debug = 2;
  } else if (req.indexOf(F("/status")) != -1) {
    client.printf("{\"switches\": {\"light\": \"%s\"}, \"sensors\": {\"distance\": {\"state\": \"%s\", \"measurement\": %f}, \"light\": %d}, \"uptime\": \"%d:%02d:%02d:%02d\", \"debug\": %d}\n", 
        lightOn ? "on" : "off", sensorActive ? "on" : "off", currValues.averageDistance, currValues.lightLevel, currValues.uptimeDays, currValues.uptimeHours, currValues.uptimeMinutes, currValues.uptimeSeconds, debug);
  } else {
      syslog.logf("received invalid request: %s", req.c_str());
      sensorActive = digitalRead(LED_BUILTIN) ? true : false;
  } 

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }
}
