/*
    This sketch demonstrates how to set up a simple HTTP-like server.
    The server will set a GPIO pin depending on the request
      http://server_ip/gpio/0 will set the GPIO2 low,
      http://server_ip/gpio/1 will set the GPIO2 high
    server_ip is the IP address of the ESP8266 module, will be
    printed to Serial when the module is connected.
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>

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

// Pin for Relay
uint8_t relayPin = D5;       // declare external LED pin

// Distance Sensor
// vin == black
// grnd == white
const int echoPin = D6; //yellow
const int trigPin = D7; //green
float duration, distance;
float aggregattedDistance = 0;
float averageDistance = 0;
float distanceToMe = 50.0;

int iteration = 0;
int timesEmpty = 0;
int sampleSize = 20;
// Start with the light off and the relay on
bool switchActive = true;
bool lightOn = false;
int debug = 0;

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
  
  digitalWrite(relayPin, LOW);
}

void controlLight() {
  iteration++;

  // Check the sensor to see if I'm at my desk

  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration/2)/74.052;
  if (debug == 2) {
    syslog.logf("Iteration: %d; Distance: %f", iteration, distance);
  }

  // Work with an average over a bunch of samples
  aggregattedDistance = aggregattedDistance + distance;
  if (iteration == sampleSize) {
    averageDistance = aggregattedDistance / sampleSize;
    if (debug) {
      syslog.logf("Average distance: %f\"", averageDistance);
    }
    aggregattedDistance = 0;
    iteration = 0;

    if ( !lightOn && averageDistance < distanceToMe) {
      // I'm at my desk, turn the light on
      syslog.log(LOG_INFO, "Turning light ON");
      digitalWrite(relayPin, HIGH);
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
    if (lightOn && timesEmpty > 10) {
      syslog.log(LOG_INFO, "Turning light OFF");
      digitalWrite(relayPin, LOW);
      lightOn = false;
    }
  }
  delay(10);
}

void setup() {
  Serial.begin(115200);

  // prepare LED and Relay PINs
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // prepare the Distance Sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
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
}

void loop() {
  ArduinoOTA.handle();

  if (switchActive) {
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

  // Check the request and determine what to do with the switch relay
  if (req.indexOf(F("/switch/0")) != -1) {
    syslog.log(LOG_INFO, "Turning relay switiching off");
    switchActive = false;
    // Set the variables back to initial values and close the circuit so the switch will work
    iteration = 0;
    aggregattedDistance = 0;
    timesEmpty = 0;
    lightOn = true;
    // Set the relay to closed for the manual switch to work
    digitalWrite(relayPin, HIGH);
    // Turn the onboard LED off to indicate the switching is off
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (req.indexOf(F("/switch/1")) != -1) {
    syslog.log(LOG_INFO, "Turning relay switiching on");
    switchActive = true;
    // Turn the onboard LED on to indicate the switching is on
    digitalWrite(LED_BUILTIN, LOW);
  } else if (req.indexOf(F("/status")) != -1) {
    syslog.logf(LOG_INFO, "Status: relay is %s; light is %s", switchActive ? "active" : "inactive", lightOn ? "on" : "off");
  } else if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Turning debug off");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
  } else if (req.indexOf(F("/light/0")) != -1) {
    if (lightOn) {
      syslog.log(LOG_INFO, "Turning light off");
      digitalWrite(relayPin, LOW);
      lightOn = false;
    } else {
      syslog.log(LOG_INFO, "Light is already off");
    }
  } else if (req.indexOf(F("/light/1")) != -1) {
    if (lightOn) {
      syslog.log(LOG_INFO, "Light is already on");
    } else {
      syslog.log(LOG_INFO, "Turning light on");
      digitalWrite(relayPin, HIGH);
      lightOn = false;
    }
  } else {
    syslog.log("received invalid request");
    switchActive = digitalRead(LED_BUILTIN) ? true : false;
  }

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  // Send the response to the client
  // it is OK for multiple small client.print/write,
  // because nagle algorithm will group them into one single packet
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nRelay Switch is now "));
  client.print((switchActive) ? F("active") : F("inactive"));
  client.print(F("<br><br>Click <a href='http://"));
  client.print(WiFi.localIP());
  client.print(F("/switch/1'>here</a> to switch it on, or <a href='http://"));
  client.print(WiFi.localIP());
  client.print(F("/switch/0'>here</a> to switch it off.</html>"));

  // The client will actually be *flushed* then disconnected
  // when the function returns and 'client' object is destroyed (out-of-scope)
  // flush = ensure written data are received by the other side
}
