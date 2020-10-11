#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ezTime.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_ADS1015.h>
 
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
#define DEVICE_HOSTNAME "iot-lcdi2c"
#define APP_NAME "display"
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;
Timezone myTZ;

// Pin for Relays
#define LVLIGHT_PIN 1      // (ESP-01) TX 
#define IRRIGATION_PIN 3    // (ESP-01) RX pin
bool lvRelayOn = false;
bool irrigationRealyOn = false;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Adafruit i2c analog interface
Adafruit_ADS1115 ads(0x48);

void setup() {
  // start out with the relays off
  pinMode(LVLIGHT_PIN, OUTPUT);
  digitalWrite(LVLIGHT_PIN, LOW);
  pinMode(IRRIGATION_PIN, OUTPUT);
  digitalWrite(IRRIGATION_PIN, LOW);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(ssid, password);

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

  // set I2C pins (SDA = GPIO2, SDL = CPIO0)
  Wire.begin(2, 0);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    syslog.log(F("SSD1306 allocation failed"));
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // start the i2c analog gateway
  ads.begin();
}

int16_t lightLevel = 0;
int16_t humidityLevel = 0;
int dusk = 500;
int sampleSize = 10;
int timesDark = 0;
int timesLight = 0;
bool lvLightOverride = false;
void loop() {
  ArduinoOTA.handle();

  // call the eztime events to update ntp date when it wants
  events();

  if (secondChanged()) {
    lightLevel = ads.readADC_SingleEnded(0);
    humidityLevel = ads.readADC_SingleEnded(1);

    // print some stuff to the display
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(0);
    display.setTextColor(WHITE);
    display.printf("%02d/%02d/%d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
    display.println();
    display.printf("light level: %d", lightLevel);
    display.println();
    display.printf("humidity level: %d", humidityLevel);
    display.println();
    display.display();

    if (!lvLightOverride) {
      // lv light control
      if (!lvRelayOn) {
        // if it's dark and not between midnight and 6AM, turn the lights on
        if ((lightLevel < dusk) && ( hour() <= 23 && hour() > 6)) {
          timesDark++;
          if (debug) {
            syslog.logf(LOG_INFO, "DEBUG: light level below dusk %d for %d times", dusk, timesDark);
          }
          if (timesDark > sampleSize) {
            syslog.log(LOG_INFO, "Turning LV lights on");
            digitalWrite(LVLIGHT_PIN, HIGH);
            lvRelayOn = true;
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
            syslog.log(LOG_INFO, "Turning LV lights off");
            digitalWrite(LVLIGHT_PIN, LOW);
            lvRelayOn = false;
          }
        } else {
          timesLight = 0;
        } 
      }
    } // lvLightOverride
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
  } else if (req.indexOf(F("/lvlight/override/on")) != -1) {
    syslog.log(LOG_INFO, "Overriding LV light operation");
    lvLightOverride = true;
    timesDark = 0;
    timesLight = 0;
  } else if (req.indexOf(F("/lvlight/override/off")) != -1) {
    syslog.log(LOG_INFO, "Disabling LV light override");
    lvLightOverride = false;
  } else if (req.indexOf(F("/lvlight/status")) != -1) {
    client.print(lvRelayOn);
  } else if (req.indexOf(F("/lvlight/off")) != -1) {
    if (lvRelayOn) {
      syslog.log(LOG_INFO, "Turning LV lights off");
      digitalWrite(LVLIGHT_PIN, LOW);
      lvRelayOn = false;
    } else {
      syslog.log(LOG_INFO, "LV lights are already off");
    }
  } else if (req.indexOf(F("/lvlight/on")) != -1) {
    if (!lvRelayOn) {
      syslog.log(LOG_INFO, "Turning LV lights on");
      digitalWrite(LVLIGHT_PIN, HIGH);
      lvRelayOn = true;
    } else {
      syslog.log(LOG_INFO, "LV lights are already on");
    }
  } else if (req.indexOf(F("/irrigation/status")) != -1) {
    client.print(irrigationRealyOn);
  } else if (req.indexOf(F("/irrigation/off")) != -1) {
    if (irrigationRealyOn) {
      syslog.log(LOG_INFO, "Turning irrigation off");
      digitalWrite(IRRIGATION_PIN, LOW);
      irrigationRealyOn = false;
    } else {
      syslog.log(LOG_INFO, "Irrigation is already off");
    }
  } else if (req.indexOf(F("/irrigation/on")) != -1) {
    if (!irrigationRealyOn) {
      syslog.log(LOG_INFO, "Turning irrigation on");
      digitalWrite(IRRIGATION_PIN, HIGH);
      irrigationRealyOn = true;
    } else {
      syslog.log(LOG_INFO, "Irrigation is already on");
    }
  } else if (req.indexOf(F("/status")) != -1) {
    client.printf("{\"switches\": {\"irrigation\": {\"state\":\"%s\"}, \"lvlight\": {\"state\": \"%s\", \"override\": \"%s\"}}, \"sensors\": {\"light\": %d, \"humidity\": %d}, \"debug\": %d}\n", 
				irrigationRealyOn ? "on" : "off", lvRelayOn ? "on" : "off", lvLightOverride ? "on" : "off", lightLevel, humidityLevel, debug);
  } else {
    syslog.log("received invalid request");
  }

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }
}

