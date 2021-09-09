#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#include <Wire.h>
#include <my_lcd.h>
#include <my_pir.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// Syslog server connection info
#define APP_NAME "base"
#define JSON_SIZE 200
#define MYTZ TZ_America_Los_Angeles

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

#define DHTLEFTPIN D5            // Digital pin connected to the DHT sensor
#define DHTRIGHTPIN D6            // Digital pin connected to the DHT sensor
#define DHTTYPE    DHT22     // DHT 22 (AM2302)
DHT_Unified dhtLeft(D5, DHTTYPE);
DHT_Unified dhtRight(DHTRIGHTPIN, DHTTYPE);

int debug = 0;
char msg[40];
LCD * lcd = new LCD();
PIR * pir = new PIR(D7); 

int const PIR_PIN = D2;  //yellow
int pirState = LOW;  //start with no motion detected
bool sensorActive = true;

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

void handleDisplay() {
  if (server.arg("state") == "on") {
    syslog.log(LOG_INFO, "Turning on LCD Display");
    server.send(200, "text/plain");
    lcd->setBackLight(true);
  } else if (server.arg("state") == "off") {
    syslog.log(LOG_INFO, "Turning off LCD Display");
    server.send(200, "text/plain");
    lcd->setBackLight(false);
  } else {
    server.send(404, "text/plain", "ERROR: unknown scani2c command");
  }
}

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;

  doc["LCD Backlight Status"] = lcd->state;
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

void setup() {
  // set I2C pins (SDA, CLK)
  Wire.begin(D2, D1);

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

  sprintf(msg, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  // Start the server
  // Start the server
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/display", handleDisplay);
  server.begin();

  dhtLeft.begin();
  dhtRight.begin();
  if (!lcd->begin(16, 2))  {
    syslog.log(LOG_INFO, "LCD Initialization failed");
  }
}

time_t prevTime = 0;
time_t now = 0;
bool displayOn;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  prevTime = now;
  now = time(nullptr);
  pir->handle();
  if (pir->activity() && ! lcd->state) {
    lcd->setBackLight(true);
    syslog.log(LOG_INFO, "Person detected, turningg backlight on");
  } 
  if (!pir->activity() && lcd->state) {
    lcd->setBackLight(false);
    syslog.log(LOG_INFO, "Nobody detected, turningg backlight off");
  }

  if ( ( now != prevTime ) && ( now % 5 == 0 ) ) {

    sensors_event_t event;

    // Row 1, Humdity 
    dhtLeft.humidity().getEvent(&event);
    double leftHumidity = event.relative_humidity;
    dhtRight.humidity().getEvent(&event);
    double rightHumidity = event.relative_humidity;
    char row1[16];
    sprintf(row1, "H: %5.2f%%%6.2f%%", leftHumidity, rightHumidity);
    lcd->setCursor(0, 0);
    lcd->print(row1);

    // Row 2, Temperature
    dhtLeft.temperature().getEvent(&event);
    double leftTemp = event.temperature * 9 / 5 + 32;
    dhtRight.temperature().getEvent(&event);
    double rightTemp = event.temperature * 9 / 5 + 32;
    char row2[16];
    sprintf(row2, "T: %5.2f%7.2f", leftTemp, rightTemp);
    lcd->setCursor(0, 1);
    lcd->print(row2);
  }


}
