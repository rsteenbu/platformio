#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>
#endif

#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <time.h>                       // time() ctime()

#ifdef ESP32
WebServer server(80);
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8*60*60*-1; const int   daylightOffset_sec = 3600;
#else
ESP8266WebServer server(80);
#define MYTZ TZ_America_Los_Angeles
#endif

// Syslog server connection info
#define APP_NAME "bootstrap"
#define JSON_SIZE 200

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

#include <SPI.h>
#include <Wire.h>

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

#ifndef ESP32
// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int SDA_PIN=0;
const int SCL_PIN=2;
#else
const int SDA_PIN=21;
const int SCL_PIN=22;
#endif

int debug = 0;
bool scanI2CMode = false;
char msg[40];

void setup() {
  // set I2C pins (SDA, CLK)
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.begin(115200);
  Serial.println("Booting up");

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(WIFI_SSID);
    Serial.println("...");
    delay(500);
  }

  sprintf(msg, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

#ifdef ESP32
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
#else
  configTime(MYTZ, "pool.ntp.org");
#endif

  // Start the server
  // Start the server
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/scani2c", handleScanI2C);
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
  } else if (server.arg("level") == "2") {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown debug command");
  }
}

void handleScanI2C() {
  if (server.arg("state") == "on") {
    server.send(200, "text/plain");
    scanI2CMode = true;
    syslog.log(LOG_INFO, "Turned on I2C Scanning");
  } else if (server.arg("state") == "off") {
    server.send(200, "text/plain");
    scanI2CMode = false;
    syslog.log(LOG_INFO, "Turned off I2C Scanning");
  } else {
    server.send(404, "text/plain", "ERROR: unknown scani2c command");
  }
}

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;

  doc["i2cScan"] = scanI2CMode;
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

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  time_t now = time(nullptr);
  if ( scanI2CMode ) {
    scanI2C();
    scanI2CMode = false;
  }

}

void scanI2C() {
  byte error, address;
  int nDevices;

  syslog.logf(LOG_INFO, "Starting I2C Scan"); 

  nDevices = 0;
  for(address = 0; address <= 127; address++ ) 
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      char hexaddr[10];
      sprintf(hexaddr, "0x%02x", address);
      syslog.logf(LOG_INFO, "I2C device found at address %s", hexaddr);

      nDevices++;
    }
    else if (error==4) 
    {
      char hexaddr[10];
      sprintf(hexaddr, "0x%02x", address);
      syslog.logf(LOG_INFO, "Unknown error at address %s", hexaddr);
    }    
  }

  if (nDevices == 0)
    syslog.log(LOG_INFO, "No I2C devices found\n");
  else
    syslog.log(LOG_INFO, "I2C scan done\n");

}
