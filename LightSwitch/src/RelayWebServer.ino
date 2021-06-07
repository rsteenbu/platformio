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

Vector<TimerRelay*> TimerSwitches;
TimerRelay * storage_array[8];

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

  // Setup the Relay
  TimerSwitches.setStorage(storage_array);

  const int SWITCH_COUNT = 2;
  const int MAX_START_TIMES = 5;
  const char *switchNames[SWITCH_COUNT] = { "test1", "test2" };
  int StartTimes[8][5]  
  { 
    // patio_pots
    {  7, 11, 15, 19, -1 }, // hour
    {  0,  0,  0,  0, -1 }, // minute
    // cottage
    {  7, 15, -1, -1, -1 },         // hour
    { 30, 30, -1, -1, -1 }          // minute
  };

  struct TimerConfig {
    TimerConfig(String a, int b): name(a), runTime(b) {}
    TimerConfig() {}

    String name;
    int runTime;
    int hours[MAX_START_TIMES];
    int minutes[MAX_START_TIMES];
  };


  TimerConfig* switchConfigs[2];
  switchConfigs[0] = &TimerConfig("patio_pots", 5);
  switchConfigs[1] = &TimerConfig("cottage", 15);

  switchConfigs[0]->hours =  {  7, 11, 15, 19, -1 };
  (switchConfigs[0]).minutes[] = {  0,  0,  0,  0, -1 };
  (switchConfigs[1]).hours[] =   {  7, 15, -1, -1, -1 };
  (switchConfigs[1]).minutes[] = { 30, 30, -1, -1, -1 };

  for (int n = 0; n < SWITCH_COUNT; n++) {
    TimerRelay * lightSwitch = new TimerRelay(TX_PIN);
    //lightSwitch->setup(switchNames[n]);
    lightSwitch->setup(switchConfigs[n].name);

    for (int m = 0; m < MAX_START_TIMES; m++) {
      //int hour = startTimtes[n*2][m];
      //int minute = startTimtes[n*2+1][m];
      int hour = switchConfigs[n].hours[m];
      int minute = switchConfigs[n].minutes[m];
      if ( hour != -1 ) lightSwitch->setStartTime(hour, minute);
    }


    TimerSwitches.push_back(lightSwitch);
  }

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
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

  for (TimerRelay * lightSwitch : TimerSwitches) {
    switches[lightSwitch->name]["state"] = lightSwitch->state();
    switches[lightSwitch->name]["override"] = lightSwitch->scheduleOverride;
    switches[lightSwitch->name]["Time Left"] = lightSwitch->timeLeftToRun;
    switches[lightSwitch->name]["Last Run Time"] = lightSwitch->prettyOnTime;
    switches[lightSwitch->name]["Next Run Time"] = lightSwitch->nextTimeToRun;
  }
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

void handleLight() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", (TimerSwitches[0])->on ? "1" : "0");
  } else if (server.arg("state") == "on") {
    syslog.logf(LOG_INFO, "Turning light on at %ld", (TimerSwitches[0])->onTime);
    (TimerSwitches[0])->switchOn();
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    syslog.logf(LOG_INFO, "Turning light off at %ld", (TimerSwitches[0])->offTime);
    (TimerSwitches[0])->switchOff();
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn light command");
  }
}

time_t prevTime = 0;;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  time_t now = time(nullptr);
  if ( now != prevTime ) {
    if ( (TimerSwitches[0])->handle() ) {
      syslog.logf(LOG_INFO, "%s %s", (TimerSwitches[0])->name, (TimerSwitches[0])->state());
    }
  }
  prevTime = now;
}
