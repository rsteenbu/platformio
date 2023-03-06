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

#include <ws2812fx.h>
#define LED_COUNT 30
#define LED_PIN 5
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);

#include <my_relay.h>
Relay * LED_Switch = new Relay(4);

#ifdef ESP32
WebServer server(80);
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 8*60*60*-1;
const int   daylightOffset_sec = 3600;
#else
ESP8266WebServer server(80);
#define MYTZ TZ_America_Los_Angeles
#endif

// Syslog server connection info
#define APP_NAME "bootstrap"
#define JSON_SIZE 400

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;
char msg[40];

void setup() {
  Serial.begin(115200);
  Serial.println("Booting up");

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to network ");
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

  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  //TODO make the url thingy showable in status
  server.on("/ledswitch", handleLedStrip);
  server.begin();

  LED_Switch->setup("ledswitch");

  ws2812fx.init();
  ws2812fx.setBrightness(32);

  // create two active segments
  //ws2812fx.setSegment(0, 0, LED_COUNT-1, FX_MODE_RAINBOW_CYCLE, BLACK, 5000, NO_OPTIONS);
  ws2812fx.setSegment(0, 0, LED_COUNT-1, FX_MODE_STATIC, BLACK, 3000, NO_OPTIONS);

  // create additional "idle" segments that will be activated later
  //ws2812fx.setIdleSegment(1, 0, LED_COUNT-1, FX_MODE_TWINKLEFOX, BLACK, 3000, NO_OPTIONS);
  //ws2812fx.setIdleSegment(2, 0, LED_COUNT-1, FX_MODE_MERRY_CHRISTMAS, BLACK, 2000, NO_OPTIONS);

  ws2812fx.setIdleSegment(1, 0, LED_COUNT-1, FX_MODE_BLINK, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(2, 0, LED_COUNT-1, FX_MODE_BREATH, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(3, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE, BLACK, 3000, NO_OPTIONS);
  /*
  ws2812fx.setIdleSegment(4, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_INV, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(5, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_REV, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(6, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_REV_INV, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(7, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(8, 0, LED_COUNT-1, FX_MODE_RANDOM_COLOR, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(9, 0, LED_COUNT-1, FX_MODE_SINGLE_DYNAMIC, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(10, 0, LED_COUNT-1, FX_MODE_MULTI_DYNAMIC, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(11, 0, LED_COUNT-1, FX_MODE_RAINBOW, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(12, 0, LED_COUNT-1, FX_MODE_RAINBOW_CYCLE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(13, 0, LED_COUNT-1, FX_MODE_SCAN, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(14, 0, LED_COUNT-1, FX_MODE_DUAL_SCAN, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(15, 0, LED_COUNT-1, FX_MODE_FADE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(16, 0, LED_COUNT-1, FX_MODE_THEATER_CHASE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(17, 0, LED_COUNT-1, FX_MODE_THEATER_CHASE_RAINBOW, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(18, 0, LED_COUNT-1, FX_MODE_RUNNING_LIGHTS, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(19, 0, LED_COUNT-1, FX_MODE_TWINKLE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(20, 0, LED_COUNT-1, FX_MODE_TWINKLE_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(21, 0, LED_COUNT-1, FX_MODE_TWINKLE_FADE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(22, 0, LED_COUNT-1, FX_MODE_TWINKLE_FADE_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(23, 0, LED_COUNT-1, FX_MODE_SPARKLE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(24, 0, LED_COUNT-1, FX_MODE_FLASH_SPARKLE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(25, 0, LED_COUNT-1, FX_MODE_HYPER_SPARKLE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(26, 0, LED_COUNT-1, FX_MODE_STROBE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(27, 0, LED_COUNT-1, FX_MODE_STROBE_RAINBOW, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(28, 0, LED_COUNT-1, FX_MODE_MULTI_STROBE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(29, 0, LED_COUNT-1, FX_MODE_BLINK_RAINBOW, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(30, 0, LED_COUNT-1, FX_MODE_CHASE_WHITE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(31, 0, LED_COUNT-1, FX_MODE_CHASE_COLOR, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(32, 0, LED_COUNT-1, FX_MODE_CHASE_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(33, 0, LED_COUNT-1, FX_MODE_CHASE_RAINBOW, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(34, 0, LED_COUNT-1, FX_MODE_CHASE_FLASH, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(35, 0, LED_COUNT-1, FX_MODE_CHASE_FLASH_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(36, 0, LED_COUNT-1, FX_MODE_CHASE_RAINBOW_WHITE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(37, 0, LED_COUNT-1, FX_MODE_CHASE_BLACKOUT, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(38, 0, LED_COUNT-1, FX_MODE_CHASE_BLACKOUT_RAINBOW, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(39, 0, LED_COUNT-1, FX_MODE_COLOR_SWEEP_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(40, 0, LED_COUNT-1, FX_MODE_RUNNING_COLOR, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(41, 0, LED_COUNT-1, FX_MODE_RUNNING_RED_BLUE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(42, 0, LED_COUNT-1, FX_MODE_RUNNING_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(43, 0, LED_COUNT-1, FX_MODE_LARSON_SCANNER, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(44, 0, LED_COUNT-1, FX_MODE_COMET, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(45, 0, LED_COUNT-1, FX_MODE_FIREWORKS, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(46, 0, LED_COUNT-1, FX_MODE_FIREWORKS_RANDOM, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(47, 0, LED_COUNT-1, FX_MODE_MERRY_CHRISTMAS, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(48, 0, LED_COUNT-1, FX_MODE_FIRE_FLICKER, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(49, 0, LED_COUNT-1, FX_MODE_FIRE_FLICKER_SOFT, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(50, 0, LED_COUNT-1, FX_MODE_FIRE_FLICKER_INTENSE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(51, 0, LED_COUNT-1, FX_MODE_CIRCUS_COMBUSTUS, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(52, 0, LED_COUNT-1, FX_MODE_HALLOWEEN, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(53, 0, LED_COUNT-1, FX_MODE_BICOLOR_CHASE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(54, 0, LED_COUNT-1, FX_MODE_TRICOLOR_CHASE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(55, 0, LED_COUNT-1, FX_MODE_TWINKLEFOX, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(56, 0, LED_COUNT-1, FX_MODE_RAIN, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(57, 0, LED_COUNT-1, FX_MODE_BLOCK_DISSOLVE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(58, 0, LED_COUNT-1, FX_MODE_ICU, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(59, 0, LED_COUNT-1, FX_MODE_DUAL_LARSON, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(60, 0, LED_COUNT-1, FX_MODE_RUNNING_RANDOM2, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(61, 0, LED_COUNT-1, FX_MODE_FILLER_UP, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(62, 0, LED_COUNT-1, FX_MODE_RAINBOW_LARSON, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(63, 0, LED_COUNT-1, FX_MODE_RAINBOW_FIREWORKS, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(64, 0, LED_COUNT-1, FX_MODE_TRIFADE, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(65, 0, LED_COUNT-1, FX_MODE_VU_METER, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(66, 0, LED_COUNT-1, FX_MODE_HEARTBEAT, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(67, 0, LED_COUNT-1, FX_MODE_BITS, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(68, 0, LED_COUNT-1, FX_MODE_MULTI_COMET, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(69, 0, LED_COUNT-1, FX_MODE_FLIPBOOK, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(70, 0, LED_COUNT-1, FX_MODE_POPCORN, BLACK, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(71, 0, LED_COUNT-1, FX_MODE_OSCILLATOR, BLACK, 3000, NO_OPTIONS);
*/

  ws2812fx.start();

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

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

  switches[LED_Switch->name]["state"] = LED_Switch->state();
  switches[LED_Switch->name]["Last On Time"] = LED_Switch->prettyOnTime;
  switches[LED_Switch->name]["Last Off Time"] = LED_Switch->prettyOffTime;
  uint8_t mode = ws2812fx.getMode(); 
  switches[LED_Switch->name]["Mode"] = ws2812fx.getModeName(mode);

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

void handleLedStrip() {
  if (server.arg("state") == "on") {
    LED_Switch->switchOn();
    syslog.logf(LOG_INFO, "Turned %s on", LED_Switch->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    LED_Switch->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off", LED_Switch->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", LED_Switch->on ? "1" : "0");
  } else if (server.arg("state") == "switchMode") {
    
    uint8_t oldSegment = *(ws2812fx.getActiveSegments());
    uint8_t oldMode = ws2812fx.getMode(oldSegment); 
    uint8_t newSegment = ( oldSegment + 1 ) % ws2812fx.getNumSegments(); 
    uint8_t newMode = ws2812fx.getMode(newSegment); 

    ws2812fx.swapActiveSegment(oldSegment, newSegment);

    syslog.logf(LOG_INFO, "old seg %d; old mode %d; new seg %d; new mode: %d; new mode name: %s", 
	oldSegment, oldMode, newSegment, newMode, ws2812fx.getModeName(newMode));

    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown command");
  }
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  ws2812fx.service();

  time_t now = time(nullptr);

}

