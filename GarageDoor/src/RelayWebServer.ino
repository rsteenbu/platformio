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
#define DEVICE_HOSTNAME "iot-garagedoor"
#define APP_NAME "garagedoor"
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;

// Pin for Relay
//uint8_t RELAY_PIN = 2;       // Use the TX pin to control the relay
uint8_t RELAY_PIN = D1;
uint8_t REED_OPEN_PIN = D2;
uint8_t REED_CLOSED_PIN = D3;
uint8_t LED_OPEN_PIN = D6;
uint8_t LED_CLOSED_PIN = D5;

// syslog control messages
int DOOR_OPEN = 0;
int DOOR_OPENING = 1;
int DOOR_CLOSED = 2;
int DOOR_CLOSING = 3;
int doorStatus;

// Distance Sensor
// vin == black
// grnd == white
bool relayOn = false;

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

void setup() {
  Serial.begin(115200);

  // Since the other end of the reed switch is connected to ground, we need
  // to pull-up the reed switch pin internally.
  pinMode(REED_OPEN_PIN, INPUT_PULLUP);
  pinMode(LED_OPEN_PIN, OUTPUT);
  pinMode(REED_CLOSED_PIN, INPUT_PULLUP);
  pinMode(LED_CLOSED_PIN, OUTPUT);

  // prepare LED and Relay PINs
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  syslog.logf(LOG_INFO, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());

  // Setup OTA Update
  setupOTA();

  // Start the server
  server.begin();
}

void loop() {
  ArduinoOTA.handle();

  int doorOpen = digitalRead(REED_OPEN_PIN); // Check to see of the door is open
  if (doorOpen == LOW) { // Door detected is in the open position
    if (doorStatus != DOOR_OPEN) {
      syslog.log(LOG_INFO, "Garage door detected open");
      digitalWrite(LED_OPEN_PIN, HIGH); // Turn the LED on
      doorStatus = DOOR_OPEN;
    }
  } else { // Door is not in the open position
    if (doorStatus == DOOR_OPEN ) {
      syslog.log(LOG_INFO, "Garage door closing");
      digitalWrite(LED_OPEN_PIN, LOW); // Turn the LED off
      doorStatus = DOOR_CLOSING;
    }
  }
  
  int doorClosed = digitalRead(REED_CLOSED_PIN); // Check to see of the door is closed
  if (doorClosed == LOW) // Door detected in the closed position
  {
    if (doorStatus != DOOR_CLOSED) {
      syslog.log(LOG_INFO, "Garage door detected closed");
      digitalWrite(LED_CLOSED_PIN, HIGH); // Turn the LED on
      doorStatus = DOOR_CLOSED;
    }
  } else { // Door is not in the closed position
    if (doorStatus == DOOR_CLOSED) {
      syslog.log(LOG_INFO, "Garage door opening");
      digitalWrite(LED_CLOSED_PIN, LOW); // Turn the LED off
      doorStatus = DOOR_OPENING;
    }
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

  // Check the request and determine what to do with the switch relay
  if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Turning debug off");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
  } else if (req.indexOf(F("/status")) != -1) {
    switch (doorStatus) {
      case 0:
        client.print("OPEN");
        break;
      case 1:
        client.print("OPENING");
        break;
      case 2:
        client.print("CLOSED");
        break;
      case 3:
        client.print("CLOSING");
        break;
    }
  } else if (req.indexOf(F("/operate")) != -1) {
    syslog.log(LOG_INFO, "Operating garagedoor");
    if (debug == 1) {
      syslog.log(LOG_INFO, "Turning garagedoor relay on");
    }
    digitalWrite(RELAY_PIN, HIGH);
    delay(100);
    if (debug == 1) {
      syslog.log(LOG_INFO, "Turning garagedoor relay off");
    }
    digitalWrite(RELAY_PIN, LOW);
    client.print("OK");
  } else {
    syslog.logf("Received invalid request: %s", req.c_str());
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
