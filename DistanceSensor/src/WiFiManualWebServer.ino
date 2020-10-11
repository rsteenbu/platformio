#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <Wire.h>

#include <ArduinoOTA.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ezTime.h>

#ifndef STASSID
#define STASSID "***REMOVED***"
#define STAPSK  "***REMOVED***"
#endif

// Create an instance of the server
// specify the port to listen on as an argument
const char* ssid = STASSID;
const char* password = STAPSK;
WiFiServer server(80);

// Syslog server connection info
#define SYSLOG_SERVER "ardupi4"
#define SYSLOG_PORT 514
// This device info
#define DEVICE_HOSTNAME "iot-computerroom"
#define APP_NAME "lightswitch"
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;

// Pin for Relay
#define RELAY_PIN  D3

// Declaration for SSD1351 display connected using software SPI (default case):
#define RST_PIN   D1
#define DC_PIN    D6
#define SCLK_PIN  D5
#define MOSI_PIN  D7
#define CS_PIN    D8

// distance sensory Pins
//yellow
#define ECHO_PIN D2
//green
#define TRIG_PIN D4

// Distance Sensor
// vin == black
// grnd == white
float duration, distance;
float aggregattedDistance = 0;
float averageDistance = 0;
float distanceToMe = 40.0;
int iterationsBeforeOff = 100;
int sampleSize = 20;
bool sensorActive = true;
bool lightOn = false;
int lightLevel = 0;

int iteration = 0;
int timesEmpty = 0;
int seconds;

Timezone myTZ;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels

// Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0  
#define WHITE           0xFFFF

Adafruit_SSD1351 oled = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, CS_PIN, DC_PIN, MOSI_PIN, SCLK_PIN, RST_PIN);  
//Adafruit_SSD1351 oled = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, CS_PIN, DC_PIN, RST_PIN);

void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

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
    averageDistance = aggregattedDistance / sampleSize;
    if (debug) {
      syslog.logf("Average distance: %f\"", averageDistance);
    }
    aggregattedDistance = 0;

    if ( !lightOn && averageDistance < distanceToMe) {
      // I'm at my desk, turn the light on
      syslog.log(LOG_INFO, "Person detected, turning light on");
      digitalWrite(RELAY_PIN, LOW);
      timesEmpty = 0;
      lightOn = true;
    } else {
      // if the lights on, let's make sure I'm really not at my desk
      if (averageDistance > distanceToMe) {
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
  Serial.begin(115200);

  // prepare LED and Relay PINs
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // prepare the Distance Sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

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
  setupOTA();

  // Start the server
  server.begin();
  Serial.println(F("Server started"));

  // Print the IP address
  Serial.println(WiFi.localIP());

  //Set the time
  waitForSync();
  myTZ.setLocation(F("America/Los_Angeles"));

  //initialize the display
  oled.begin();
  oled.fillScreen(BLACK);
  oled.print("light level\nuptime (s)");
  oled.fillRect(70, 0, 40, 8, RED);
  oled.fillRect(70, 9, 40, 8, RED);

}

int prevValues[3];;
String prevDateTimes[2];
void loop() {
  ArduinoOTA.handle();

  // call the eztime events to update ntp date when it wants
  events();

  // make sure we don't overflow iteration
  if (iteration == 100000) {
    iteration = 0;
  }

  iteration++;

  seconds = (int) millis() / 1000;
//  if (seconds >= prevValues[1] + 1) {
  if (secondChanged()) {
    // airValue = 785
    // waterValue = 470
    // soilmoisturepercent = map(soilMoistureValue, airValue, waterValue, 0, 100);
    lightLevel = analogRead(A0); 
    String currDate = myTZ.dateTime("m/d/y");
    String currTime = myTZ.dateTime("H:i");
    int currSeconds = (int) myTZ.second();

    oled.setTextSize(0);

    oled.setTextColor(BLACK, RED);
    oled.setCursor(75,0);
    oled.print(prevValues[0]);
    oled.setCursor(75,9);
    oled.print(prevValues[1]);
    oled.setTextColor(WHITE, RED);
    oled.setCursor(75,0);
    oled.print(lightLevel);
    oled.setCursor(75,9);
    oled.print(seconds);

    //print the date
    if (currDate != prevDateTimes[0]) {
      oled.setCursor(0,20);
      oled.setTextColor(BLACK);
      oled.print(prevDateTimes[0]);
      oled.setCursor(0,20);
      oled.setTextColor(WHITE);
      oled.print(currDate);
    }
    
    //print the hour and minute
    if (currTime != prevDateTimes[1]) {
      oled.setCursor(60,20);
      oled.setTextColor(BLACK);
      oled.print(prevDateTimes[1]);
      oled.setCursor(60,20);
      oled.setTextColor(WHITE);
      oled.print(currTime);
    } 
    
    //print the second
      oled.setCursor(88,20);
      oled.setTextColor(BLACK);
      oled.printf(":%02d", prevValues[2]);
      oled.setCursor(88,20);
      oled.setTextColor(WHITE);
      oled.printf(":%02d", currSeconds);

    prevValues[0] = lightLevel;
    prevValues[1] = seconds;
    prevValues[2] = currSeconds;
    prevDateTimes[0] = currDate;
    prevDateTimes[1] = currTime;
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
    syslog.logf(LOG_INFO, "Status: relay is %s; light is %s", sensorActive ? "active" : "inactive", lightOn ? "on" : "off");
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
