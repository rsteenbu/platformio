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

#include "config_default.h"

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, SYSLOG_APP_NAME, LOG_LOCAL0);

int debug = DEBUG;;

// ESP-01 Pins

Relay * powerswitch = new Relay(RELAY_PIN);

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

  powerswitch->setup("relay");

  // Start the server
  server.on("/help", handleHelp);
  server.on("/debug", handleDebug);
  server.on("/power", handleSwitch);
  server.on("/status", handleStatus);
  server.begin();
}

void handleHelp() {
  String helpMessage = "/help";

  helpMessage += "/debug?level=status\n";
  helpMessage += "/debug?level=[012]\n";
  helpMessage += "/status\n";
  helpMessage += "\n";

  helpMessage += "/power?state=on\n";
  helpMessage += "/power?state=off\n";

  server.send(200, "text/plain", helpMessage);
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

  switches[powerswitch->name]["state"] = powerswitch->state();
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

void handleSwitch() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", (powerswitch->on ? "1" : "0"));
  } else if (server.arg("state") == "on") {
    powerswitch->switchOn();
    syslog.logf(LOG_INFO, "Turned %s on", powerswitch->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    powerswitch->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off", powerswitch->name);
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn switch command");
  }
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

}
