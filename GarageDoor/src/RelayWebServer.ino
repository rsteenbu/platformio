#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <my_relay.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

// This device info
#define APP_NAME "garagedoor"
#define JSON_SIZE 500
#define MYTZ TZ_America_Los_Angeles

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;

const uint8_t RELAY_PIN = D1;
const uint8_t REED_OPEN_PIN = D2;
const uint8_t REED_CLOSED_PIN = D3;
const uint8_t LED_OPEN_PIN = D6;
const uint8_t LED_CLOSED_PIN = D5;
GarageDoorRelay * garageDoor = new GarageDoorRelay(RELAY_PIN, REED_OPEN_PIN, REED_CLOSED_PIN, LED_OPEN_PIN, LED_CLOSED_PIN);

void setup() {
  Serial.begin(115200);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to network...");
    delay(500);
  }

  char msg[40];
  sprintf(msg, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.log(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  garageDoor->setup("garage_door");

  // Start the server
  // Start the server
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/door", handleDoor);
  server.begin();
}

void handleDebug() {
  if (server.arg("level") == "status") {
    char msg[40];
    sprintf(msg, "Debug level: %d", debug);
    server.send(200, "text/plain", msg);
  } else if (server.arg("level") == "0") {
    syslog.log(LOG_INFO, "Debug level 0");
    debug = 0;
    server.send(200, "text/plain");
  } else if (server.arg("level") == "1") {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
    server.send(200, "text/plain");
  } if (server.arg("level") == "2") {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown debug command");
  }
}

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;

  JsonObject switches = doc.createNestedObject("switches");
  switches[garageDoor->name]["state"] = garageDoor->state();

  doc["debug"] = debug;

  char timeString[20];
  struct tm *timeinfo = localtime(&now);
  strftime (timeString,20,"%D %T",timeinfo);
  doc["time"] = timeString;

  size_t jsonDocSize = measureJsonPretty(doc);
  if (jsonDocSize > JSON_SIZE) {
    char msg[40];
    sprintf(msg, "ERROR: JSON message too long, %d", jsonDocSize);
    server.send(500, "text/plain", msg);
  } else {
    String httpResponse;
    serializeJsonPretty(doc, httpResponse);
    server.send(500, "text/plain", httpResponse);
  }
}

void handleDoor() {
  if (server.arg("command") == "status") {
    server.send(200, "text/plain", garageDoor->state());
  } else if (server.arg("command") == "operate") {
    syslog.log(LOG_INFO, "Operating Garage Door");
    garageDoor->operate();
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn light command");
  }
}

void loop() {
  ArduinoOTA.handle();

  if (garageDoor->handle()) {
    syslog.logf(LOG_INFO, "Garage door %s", garageDoor->state());
  }

  server.handleClient();
}
