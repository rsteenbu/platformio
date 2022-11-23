#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoJson.h>
#include <Timezone.h> 
#include <WiFiEspAT.h>
#include <ArduinoHttpServer.h>

#include <my_ntp.h>
#include <teensy_relay.h>

int debug = 0;
teensyRelay * XmasTree = new teensyRelay(22);

WiFiServer server(80);
WiFiUdpSender udpClient;
WiFiUDP Udp;

NTP ntp;

// This device info
const char* APP_NAME = "system";
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

#define JSON_SIZE 700
#define MAX_REQUEST_SIZE 1024

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
    Serial.print("Communication with WiFi module failed!");
    Serial.println(WiFi.status());
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
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);

  // start the http webserver
  server.begin();

  char hostname[40];
  WiFi.hostname(hostname);
  Serial.print("Hostname set to: ");
  Serial.println(hostname);
  IPAddress ip = WiFi.localIP();
  syslog.logf(LOG_INFO, "%s Alive! at IP: %d.%d.%d.%d", hostname, ip[0], ip[1], ip[2], ip[3]);

  Udp.begin(2390);
  // wait to see if a reply is available
  delay(1000);

  int tries = 0;
  while (! ntp.setup(Udp) && tries++ < 5) {
    if (debug) {
      syslog.logf(LOG_INFO, "Getting NTP time failed, try %d", tries);
    }
    delay(1000);
  }
  if (tries == 5) {
     syslog.log(LOG_INFO, "ERROR, time not set from NTP server");
  }

  // Set the system time from the NTP epoch
  setSyncInterval(300);
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

void handleStatus(WiFiClient& client) {
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

    ArduinoHttpServer::StreamHttpErrorReply httpReply(client, "text/html", "500");
    httpReply.send( msg );

  } else {
    String httpResponse;
    serializeJsonPretty(doc, httpResponse);

    ArduinoHttpServer::StreamHttpReply httpReply(client, "application/json");
    httpReply.send(httpResponse);
  }
}

void handleNotFound(WiFiClient& client) {
  Serial.println("Handling not found http request");
  String message = F("File Not Found\n\n");

  ArduinoHttpServer::StreamHttpErrorReply httpReply(client, "text/html", "400");
  httpReply.send( message );
}

void handleTree(ArduinoHttpServer::StreamHttpRequest<1024>& httpRequest, WiFiClient& client) {
  if (httpRequest.getResource()[1] == "status") {
    Serial.println("Returning status for Tree state");
    ArduinoHttpServer::StreamHttpReply httpReply(client, "text/html");
    String msg = XmasTree->state();
    httpReply.send(msg);
  } else if (httpRequest.getResource()[1] == "switchOn") {
    XmasTree->switchOn();
    Serial.println("Turned tree on");
    ArduinoHttpServer::StreamHttpReply httpReply(client, "text/html");
    httpReply.send("");
    syslog.logf(LOG_INFO, "Turned %s on", XmasTree->name);
  } else if (httpRequest.getResource()[1] == "switchOff") {
    XmasTree->switchOff();
    Serial.println("Turned tree off");
    ArduinoHttpServer::StreamHttpReply httpReply(client, "text/html");
    httpReply.send("");
    syslog.logf(LOG_INFO, "Turned %s off", XmasTree->name);
  } else {
    String msg = "ERROR: uknown tree command: ";
    msg += httpRequest.getResource()[1];
    msg += "\n\n";
    ArduinoHttpServer::StreamHttpErrorReply httpReply(client, "text/html", "400");
    httpReply.send(msg);
  }
}

void loop() {

   WiFiClient client( server.available() );

   if (client.connected()) {
      // Connected to client. Allocate and initialize StreamHttpRequest object.
      ArduinoHttpServer::StreamHttpRequest<1024> httpRequest(client);

      // Parse the request.
      if (httpRequest.readRequest()) {
         // Retrieve 2nd part of HTTP resource.
         // E.g.: "on" from "/api/sensors/on"
	 Serial.print("getResource[0]: ");
         Serial.println(httpRequest.getResource()[0]);

         if (httpRequest.getResource().toString() == "/status") {
           handleStatus(client);
         } else if (httpRequest.getResource()[0] == "tree") {
           handleTree(httpRequest, client);
	 } else {
           handleNotFound(client);
	 }
      }
      client.stop();
   }
}

