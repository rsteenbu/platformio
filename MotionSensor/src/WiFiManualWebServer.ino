#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Syslog.h>
#include <ezTime.h>

#include <my_relay.h>

const int RST_PIN  = D1; //white
const int DC_PIN   = D6; //green    Hardware SPI MISO = GPIO12 (not used)
const int SCLK_PIN = D5; //yellow   Hardware SPI CLK  = GPIO14
const int MOSI_PIN = D7; //blue     Hardware SPI MOSI = GPIO13
const int CS_PIN   = D8; //orange   Hardware SPI /CS  = GPIO15 (not used), pull-down 10k to GND

#if ADALIB == 1
// Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0  
#define WHITE           0xFFFF

#include <Wire.h>
#include <Adafruit_SSD1351.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
Adafruit_SSD1351 display = Adafruit_SSD1351(128, 128, CS_PIN, DC_PIN, MOSI_PIN, SCLK_PIN, RST_PIN);  
//Adafruit_SSD1351 display = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, CS_PIN, DC_PIN, RST_PIN);
#else
//#include <FTOLED.h>
//#include <fonts/SystemFont5x7.h>
//OLED display(CS_PIN, DC_PIN, RST_PIN);
//OLED_TextBox box(display);
#endif

// Create an instance of the server
// specify the port to listen on as an argument
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
Relay lights(D3);

//int const ECHO_PIN = D2;  //yellow
//int const TRIG_PIN = D4;  //green

// vin == black grnd == white
int const PIR_PIN = D2;  //yellow
int pirState = LOW;  //start with no motion detected
bool sensorActive = true;

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

int timesEmpty = 0;

Timezone myTZ;

void setup() {
  // prepare HW SPI Pins
  pinMode(SCLK_PIN, OUTPUT);
  pinMode(MOSI_PIN, OUTPUT);

  // prepare LED 
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // prepare the motion sensor
  pinMode(PIR_PIN, INPUT);

  lights.setup();

  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

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
  display.print("uptime:");
#else
    display.begin();
    display.selectFont(SystemFont5x7);
    display.drawString(6,70,F("System 5x7\nOn Two Lines"),RED,BLACK);
#endif
}

void loop() {
  ArduinoOTA.handle();

  // call the eztime events to update ntp date when it wants
  events();

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

    display.setTextColor(WHITE, RED);
    display.setCursor(75,0);
    display.print(currValues.lightLevel);

    if (currValues.uptimeDays != prevValues.uptimeDays) {
      display.setCursor(56,9);
      display.printf("%02d", prevValues.uptimeDays);
      display.setCursor(56,9);
      display.printf("%02d",currValues.uptimeDays);
    }
    if (currValues.uptimeHours != prevValues.uptimeHours) {
      display.setCursor(68,9);
      display.printf(":%02d", prevValues.uptimeHours);
      display.setCursor(68,9);
      display.printf(":%02d",currValues.uptimeHours);
    }
    if (currValues.uptimeMinutes != prevValues.uptimeMinutes) {
      display.setCursor(86,9);
      display.printf(":%02d", prevValues.uptimeMinutes);
      display.setCursor(86,9);
      display.printf(":%02d",currValues.uptimeMinutes);
    }
    if (currValues.uptimeSeconds != prevValues.uptimeSeconds) {
      display.setCursor(104,9);
      display.printf(":%02d", prevValues.uptimeSeconds);
      display.setCursor(104,9);
      display.printf(":%02d",currValues.uptimeSeconds);
    }

    //print the date
    if (currValues.date != prevValues.date) {
      display.setCursor(0,18);
      display.setTextColor(BLACK);
      display.print(prevValues.date);
      display.setCursor(0,18);
      display.setTextColor(WHITE);
      display.print(currValues.date);
    }
    
    //print the hour and minute
    if (currValues.time != prevValues.time) {
      display.setCursor(60,18);
      display.setTextColor(BLACK);
      display.print(prevValues.time);
      display.setCursor(60,18);
      display.setTextColor(WHITE);
      display.print(currValues.time);
    } 
    
    //print the second
    display.setCursor(88,18);
    display.setTextColor(BLACK);
    display.printf(":%02d", prevValues.timeSeconds);
    display.setCursor(88,18);
    display.setTextColor(WHITE);
    display.printf(":%02d", currValues.timeSeconds);

    prevValues = currValues;

#endif
    if (sensorActive) {
      int val = digitalRead(PIR_PIN);  // read input value from the motion sensor
      if (val == HIGH) {            // check if the input is HIGH
	if (pirState == LOW) {
	  // we have just turned on
	  syslog.logf(LOG_INFO, "Person detected, turned %s on", lights->name);
	  lights.switchOn();
	  pirState = HIGH;
	}
      } else {
	if (pirState == HIGH){
	  lights.switchOff();
	  syslog.logf(LOG_INFO, "Nobody detected, turned %s off", lights->name);
	  Serial.println("Motion ended!");
	  pirState = LOW;
	}
      } // val
    } //sensorActive
  } //secondsChanged


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
    lights.switchOn();
    syslog.logf(LOG_INFO, "Turned %s on by API request", lights->name);
  } else if (req.indexOf(F("/light/off")) != -1) {
    lights.switchOff();
    syslog.logf(LOG_INFO, "Turned %s off by API request", lights->name);
  } else if (req.indexOf(F("/light/status")) != -1) {
    if (debug == 2) {
      syslog.logf(LOG_INFO, "Light status: %s", lights.state());
    }
    client.print(lights.on);
  } else if (req.indexOf(F("/sensor/on")) != -1) {
    sensorActive = true;
    syslog.log(LOG_INFO, "Turned motion sensor switiching on");
  } else if (req.indexOf(F("/sensor/off")) != -1) {
    sensorActive = false;
    syslog.log(LOG_INFO, "Turned motion sensor switiching off");
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
        lights.state(), sensorActive ? "on" : "off", currValues.averageDistance, currValues.lightLevel, currValues.uptimeDays, currValues.uptimeHours, currValues.uptimeMinutes, currValues.uptimeSeconds, debug);
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
