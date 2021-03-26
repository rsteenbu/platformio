#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Array.h>

#include <my_relay.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#define MYTZ TZ_America_Los_Angeles

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

// This device info
#define APP_NAME "switch"
#define JSON_SIZE 200

//typedef Array<int,7> Elements;
//const int irrigationDays[7] = {0,2,4,6};

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


char msg[40];
StaticJsonDocument<200> doc;

//Relay lightSwitch(TX_PIN);
TimerRelay lightSwitch(TX_PIN);

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // Setup the Relay
  lightSwitch.setup();

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

  lightSwitch.setEveryDayOff();
  lightSwitch.setEveryOtherDayOn();
  syslog.logf(LOG_INFO, "every other day 0: %d; day 1: %d; day 2: %d; day 3: %d; day 4: %d; day 5: %d; day 6: %d", lightSwitch.checkToRun(0), lightSwitch.checkToRun(1), lightSwitch.checkToRun(2), lightSwitch.checkToRun(3), lightSwitch.checkToRun(4), lightSwitch.checkToRun(5), lightSwitch.checkToRun(6));

  // Build an array with specific days to run, 
  Array<int,7> irrigationDays;
//  irrigationDays.fill(0);
//  irrigationDays[3] = 1;  //Thursday
//  irrigationDays[6] = 1;  //Sunday
//  lightSwitch.setDaysFromArray(irrigationDays);
  lightSwitch.setEveryDayOff();  //Start with all days off
  lightSwitch.setSpecificDayOn(2);
  lightSwitch.setSpecificDayOn(5);
  lightSwitch.setSpecificDayOn(6);
  syslog.logf(LOG_INFO, "specific days 0: %d; day 1: %d; day 2: %d; day 3: %d; day 4: %d; day 5: %d; day 6: %d", lightSwitch.checkToRun(0), lightSwitch.checkToRun(1), lightSwitch.checkToRun(2), lightSwitch.checkToRun(3), lightSwitch.checkToRun(4), lightSwitch.checkToRun(5), lightSwitch.checkToRun(6));

  lightSwitch.setEveryDayOn();
  lightSwitch.setRuntime(2);
  lightSwitch.setStartTime(21, 30);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  // Start the server
  server.begin();
}

time_t now;
time_t prevTime = 0;;
void loop() {
  ArduinoOTA.handle();

  int irrigationAction = lightSwitch.handle();
  if ( irrigationAction == 1 ) {
    syslog.log(LOG_INFO, "irrigation started");
  } 
  if ( irrigationAction == 2 ) {
    syslog.log(LOG_INFO, "irrigation stopped");
  }

  now = time(nullptr);
  if ( now != prevTime ) {
    if ( now % 5 == 0 ) {
    }
  }
  prevTime = now;

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
  } else if (req.indexOf(F("/light/status")) != -1) {
    client.print(lightSwitch.on);
  } else if (req.indexOf(F("/light/off")) != -1) {
      lightSwitch.switchOff();
      syslog.logf(LOG_INFO, "Turning light off at %ld", lightSwitch.offTime);
  } else if (req.indexOf(F("/light/on")) != -1) {
      lightSwitch.switchOn();
      syslog.logf(LOG_INFO, "Turning light on at %ld", lightSwitch.onTime);
  } else if (req.indexOf(F("/status")) != -1) {
    StaticJsonDocument<JSON_SIZE> doc;
    JsonObject switches = doc.createNestedObject("switches");
    switches["light"]["state"] = lightSwitch.state();
    doc["debug"] = debug;

    char timeString[20];
    struct tm *timeinfo = localtime(&now);
    strftime (timeString,20,"%D %T",timeinfo);
    doc["time"] = timeString;
    doc["seconds"] = timeinfo->tm_sec;
    doc["wday"] = timeinfo->tm_wday;
    doc["mktime"] = mktime(timeinfo);
    doc["now"] = now;


    size_t jsonDocSize = measureJsonPretty(doc);
    if (jsonDocSize > JSON_SIZE) {
      client.printf("ERROR: JSON message too long, %d", jsonDocSize);
      client.println();
    } else {
      serializeJsonPretty(doc, client);
      client.println();
    }
  } else {
    syslog.log("received invalid request");
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


