#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Vector.h>


#include <my_relay.h>


#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#define MYTZ TZ_America_Los_Angeles


// This device info
#define APP_NAME "switch"
#define JSON_SIZE 1000

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;

// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;

Relay * lightSwitch = new Relay(TX_PIN);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting up");

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
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  lightSwitch->setup("relay");

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/light", handleLight);
  server.on("/status", handleStatus);
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
  /*
  time_t now;
  now = time(nullptr);
  */
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

  switches[lightSwitch->name]["state"] = lightSwitch->state();
  doc["debug"] = debug;

  /*
  char timeString[20];
  struct tm *timeinfo = localtime(&now);
  strftime (timeString,20,"%D %T",timeinfo);
  doc["time"] = timeString;
  */

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

void handleLight() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", (lightSwitch->on ? "1" : "0"));
  } else if (server.arg("state") == "on") {
    lightSwitch->switchOn();
    syslog.logf(LOG_INFO, "Turned %s on", lightSwitch->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    lightSwitch->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off", lightSwitch->name);
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn light command");
  }
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

}
