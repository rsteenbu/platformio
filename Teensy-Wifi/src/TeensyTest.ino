#define USE_WIFI_NINA false
#define USE_WIFI_CUSTOM true
#include <WiFiEspAT.h>
#include <WiFiWebServer.h>
#include <Syslog.h>
#include <ArduinoJson.h>
#include <Timezone.h> 
#include <TimeLib.h>

#include <my_ntp.h>
#include <teensy_relay.h>

int debug = 0;
teensyRelay * XmasTree = new teensyRelay(22);

WiFiWebServer server(80);
WiFiUdpSender udpClient;
WiFiUDP Udp;

IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
NTP ntp;

// This device info
const char* APP_NAME = "system";
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

#define JSON_SIZE 500

// US Pacific Time Zone 
TimeChangeRule usPST = {"PST", Second, Sun, Mar, 2, -420};  // Daylight time = UTC - 7 hours
TimeChangeRule usPDT = {"PDT", First, Sun, Nov, 2, -480};   // Standard time = UTC - 8 hours
Timezone usPacific(usPST, usPDT);


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial1.begin(115200);
  XmasTree->setup("xmastree");

  WiFi.init(Serial1);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  if (!WiFi.setHostname(DEVICE_HOSTNAME)) {
    Serial.println("Setting hostname failed");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // waiting for connection to Wifi network set with the SetupWiFiConnection sketch
  Serial.println("Waiting for connection to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }
  Serial.println();

  // start the http webserver
  server.begin();

  char hostname[40];
  WiFi.hostname(hostname);
  Serial.print("Hostname set to: ");
  Serial.println(hostname);
  IPAddress ip = WiFi.localIP();
  syslog.logf(LOG_INFO, "%s Alive! at IP: %d.%d.%d.%d", hostname, ip[0], ip[1], ip[2], ip[3]);

  server.on("/status", handleStatus);
  server.on("/tree", handleTree);

  Udp.begin(2390);
  // wait to see if a reply is available
  delay(1000);

  int tries = 0;
  while (! ntp.setup(timeServer, Udp) && tries++ < 5) {
    if (debug) {
      syslog.logf(LOG_INFO, "Getting NTP time failed, try %d", tries);
    }
    delay(1000);
  }
  if (tries == 5) {
     syslog.log(LOG_INFO, "ERROR, time not set from NTP server");
  }

  // Set the system time from the NTP epoch
  setSyncProvider(getTeensy3Time);
  delay(100);
  if (timeStatus()!= timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }

  if (ntp.epoch == 0) {
    syslog.log(LOG_INFO, "Setting NTP time failed");
  } else {
    time_t pacific = usPacific.toLocal(ntp.epoch);
    Teensy3Clock.set(pacific); // set the RTC
    setTime(pacific);
  }
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

void handleStatus() {
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");
  JsonObject sensors = doc.createNestedObject("sensors");

  switches[XmasTree->name]["state"] = XmasTree->state();
  switches[XmasTree->name]["Last On Time"] = XmasTree->prettyOnTime;
  switches[XmasTree->name]["Last Off Time"] = XmasTree->prettyOffTime;
  int lightLevel = 0;
  lightLevel = analogRead(A9);
  sensors["lightLevel"] = lightLevel;
  doc["debug"] = debug;

  // 11/16/21 20:17:07
  char timeString[20];
  sprintf(timeString, "%02d/%02d/%04d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
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

void handleTree() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", (XmasTree->on ? "1" : "0"));
  } else if (server.arg("state") == "on") {
    XmasTree->switchOn();
    syslog.logf(LOG_INFO, "Turned %s on", XmasTree->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    XmasTree->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off", XmasTree->name);
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn light command");
  }
}

void loop()
{
  server.handleClient();
}

