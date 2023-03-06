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

#include <my_motion.h>
MOTION * motion = new MOTION(MOTION_PIN); 

#include <Wire.h>
#include <my_lcd.h>
LCD * lcd = new LCD();

#include <Versatile_RotaryEncoder.h>
// Create a global pointer for the encoder object
Versatile_RotaryEncoder *versatile_encoder;

#include <ws2812fx.h>
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800, 72);
int colorIndex = 0;
int modeIndex = 0;
unsigned long encoderActivityTime;
int ledState = 0; // 0 runnning; 1 paused

#include <my_relay.h>
Relay * LED_Switch = new Relay(RELAY_PIN);

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

//enum colors = { red = RED, green = GREEN, blue = BLUE, white = WHITE, YELLOW, CYAN, MAGENTA, PURPLE, ORANGE, PINK, GRAY, ULTRAWHITE };

using namespace std;

class myColors {
  public:
    enum values { red = RED, green = GREEN, blue = BLUE, white = WHITE };
    int currentColorIndex;
    values index[4] = {values::red, values::green, values::blue, values::white};
    //constructors
    myColors();

    const char* currentName() {
      switch(index[currentColorIndex]) {
        case values::red   : return "red";   break;
        case values::green : return "green"; break;
        case values::blue  : return "blue";  break;
        case values::white : return "white";  break;
      }
      return "undefined";
    }

    uint32_t currentColor() {
      return index[currentColorIndex];
    }

    void nextColor() {
      currentColorIndex = (currentColorIndex + 1) % (end(index)-begin(index));
    }
    void previousColor() {
      if (currentColorIndex == 0) {
        currentColorIndex = (end(index)-begin(index)) - 1;
      } else {
        currentColorIndex = (currentColorIndex - 1) % (end(index)-begin(index));
      }
    }
};

myColors::myColors () {
  currentColorIndex = 0;
}

myColors led_color;
//Colors * led_color = new Colors();
//Relay * LED_Switch = new Relay(RELAY_PIN);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting up");

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int connectWaitTime = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to network ");
    Serial.print(WIFI_SSID);
    Serial.print(" [");
    Serial.print(connectWaitTime);
    Serial.println("s]...");
    delay(1000);
    connectWaitTime++;
  }

  sprintf(msg, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  // set I2C pins (SDA, CLK) and initialize the LCD
  Wire.begin(21, 22);
  if (!lcd->begin(LCD_COLS, LCD_ROWS))  {
    syslog.log(LOG_INFO, "LCD Initialization failed");
  }
  lcd->print(led_color.currentName());
//  lcd->cursor();
  lcd->blink();
  lcd->setCursor(0, 0);

  // Rotary Encoder setup
  versatile_encoder = new Versatile_RotaryEncoder(ROTENC_CLK, ROTENC_DT, ROTENC_SW);
  // Load to the encoder all nedded handle functions here (up to 9 functions)
  versatile_encoder->setHandleRotate(handleRotate);
  versatile_encoder->setHandlePressRotate(handlePressRotate);
  versatile_encoder->setHandlePressRotateRelease(handlePressRotateRelease);
  versatile_encoder->setHandlePress(handlePress);
  versatile_encoder->setHandleLongPress(handleLongPress);
  versatile_encoder->setLongPressDuration(1000); // default is 1000ms


#ifdef ESP32
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
#else
  configTime(MYTZ, "pool.ntp.org");
#endif

  server.on("/help", handleHelp);
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/power", handlePower);
  server.on("/modeIterate", handleModeIterate);
  server.on("/modeSetByIndex", handleModeSetByIndex);
  server.on("/colorIterate", handleColorIterate);
  server.begin();

  LED_Switch->setup("ledswitch");

  ws2812fx.init();
  ws2812fx.setBrightness(32);

  // create two active segments
  //ws2812fx.setSegment(0, 0, LED_COUNT-1, FX_MODE_RAINBOW_CYCLE, BLACK, 5000, NO_OPTIONS);
  ws2812fx.setSegment(0, 0, LED_COUNT-1, FX_MODE_STATIC, WHITE, 2000, NO_OPTIONS);

  // create additional "idle" segments that will be activated later
  //ws2812fx.setIdleSegment(1, 0, LED_COUNT-1, FX_MODE_TWINKLEFOX, BLACK, 3000, NO_OPTIONS);
  //ws2812fx.setIdleSegment(2, 0, LED_COUNT-1, FX_MODE_MERRY_CHRISTMAS, BLACK, 2000, NO_OPTIONS);
  const uint32_t colors[3] = {RED, GREEN, BLUE};

  ws2812fx.setIdleSegment(1, 0, LED_COUNT-1, FX_MODE_BLINK, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(2, 0, LED_COUNT-1, FX_MODE_BREATH, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(3, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(4, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_INV, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(5, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_REV, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(6, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_REV_INV, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(7, 0, LED_COUNT-1, FX_MODE_COLOR_WIPE_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(8, 0, LED_COUNT-1, FX_MODE_RANDOM_COLOR, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(9, 0, LED_COUNT-1, FX_MODE_SINGLE_DYNAMIC, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(10, 0, LED_COUNT-1, FX_MODE_MULTI_DYNAMIC, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(11, 0, LED_COUNT-1, FX_MODE_RAINBOW, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(12, 0, LED_COUNT-1, FX_MODE_RAINBOW_CYCLE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(13, 0, LED_COUNT-1, FX_MODE_SCAN, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(14, 0, LED_COUNT-1, FX_MODE_DUAL_SCAN, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(15, 0, LED_COUNT-1, FX_MODE_FADE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(16, 0, LED_COUNT-1, FX_MODE_THEATER_CHASE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(17, 0, LED_COUNT-1, FX_MODE_THEATER_CHASE_RAINBOW, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(18, 0, LED_COUNT-1, FX_MODE_RUNNING_LIGHTS, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(19, 0, LED_COUNT-1, FX_MODE_TWINKLE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(20, 0, LED_COUNT-1, FX_MODE_TWINKLE_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(21, 0, LED_COUNT-1, FX_MODE_TWINKLE_FADE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(22, 0, LED_COUNT-1, FX_MODE_TWINKLE_FADE_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(23, 0, LED_COUNT-1, FX_MODE_SPARKLE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(24, 0, LED_COUNT-1, FX_MODE_FLASH_SPARKLE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(25, 0, LED_COUNT-1, FX_MODE_HYPER_SPARKLE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(26, 0, LED_COUNT-1, FX_MODE_STROBE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(27, 0, LED_COUNT-1, FX_MODE_STROBE_RAINBOW, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(28, 0, LED_COUNT-1, FX_MODE_MULTI_STROBE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(29, 0, LED_COUNT-1, FX_MODE_BLINK_RAINBOW, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(30, 0, LED_COUNT-1, FX_MODE_CHASE_WHITE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(31, 0, LED_COUNT-1, FX_MODE_CHASE_COLOR, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(32, 0, LED_COUNT-1, FX_MODE_CHASE_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(33, 0, LED_COUNT-1, FX_MODE_CHASE_RAINBOW, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(34, 0, LED_COUNT-1, FX_MODE_CHASE_FLASH, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(35, 0, LED_COUNT-1, FX_MODE_CHASE_FLASH_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(36, 0, LED_COUNT-1, FX_MODE_CHASE_RAINBOW_WHITE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(37, 0, LED_COUNT-1, FX_MODE_CHASE_BLACKOUT, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(38, 0, LED_COUNT-1, FX_MODE_CHASE_BLACKOUT_RAINBOW, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(39, 0, LED_COUNT-1, FX_MODE_COLOR_SWEEP_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(40, 0, LED_COUNT-1, FX_MODE_RUNNING_COLOR, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(41, 0, LED_COUNT-1, FX_MODE_RUNNING_RED_BLUE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(42, 0, LED_COUNT-1, FX_MODE_RUNNING_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(43, 0, LED_COUNT-1, FX_MODE_LARSON_SCANNER, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(44, 0, LED_COUNT-1, FX_MODE_COMET, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(45, 0, LED_COUNT-1, FX_MODE_FIREWORKS, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(46, 0, LED_COUNT-1, FX_MODE_FIREWORKS_RANDOM, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(47, 0, LED_COUNT-1, FX_MODE_MERRY_CHRISTMAS, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(48, 0, LED_COUNT-1, FX_MODE_FIRE_FLICKER, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(49, 0, LED_COUNT-1, FX_MODE_FIRE_FLICKER_SOFT, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(50, 0, LED_COUNT-1, FX_MODE_FIRE_FLICKER_INTENSE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(51, 0, LED_COUNT-1, FX_MODE_CIRCUS_COMBUSTUS, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(52, 0, LED_COUNT-1, FX_MODE_HALLOWEEN, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(53, 0, LED_COUNT-1, FX_MODE_BICOLOR_CHASE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(54, 0, LED_COUNT-1, FX_MODE_TRICOLOR_CHASE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(55, 0, LED_COUNT-1, FX_MODE_TWINKLEFOX, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(56, 0, LED_COUNT-1, FX_MODE_RAIN, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(57, 0, LED_COUNT-1, FX_MODE_BLOCK_DISSOLVE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(58, 0, LED_COUNT-1, FX_MODE_ICU, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(59, 0, LED_COUNT-1, FX_MODE_DUAL_LARSON, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(60, 0, LED_COUNT-1, FX_MODE_RUNNING_RANDOM2, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(61, 0, LED_COUNT-1, FX_MODE_FILLER_UP, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(62, 0, LED_COUNT-1, FX_MODE_RAINBOW_LARSON, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(63, 0, LED_COUNT-1, FX_MODE_RAINBOW_FIREWORKS, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(64, 0, LED_COUNT-1, FX_MODE_TRIFADE, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(65, 0, LED_COUNT-1, FX_MODE_VU_METER, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(66, 0, LED_COUNT-1, FX_MODE_HEARTBEAT, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(67, 0, LED_COUNT-1, FX_MODE_BITS, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(68, 0, LED_COUNT-1, FX_MODE_MULTI_COMET, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(69, 0, LED_COUNT-1, FX_MODE_FLIPBOOK, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(70, 0, LED_COUNT-1, FX_MODE_POPCORN, colors, 3000, NO_OPTIONS);
  ws2812fx.setIdleSegment(71, 0, LED_COUNT-1, FX_MODE_OSCILLATOR, colors, 3000, NO_OPTIONS);

  ws2812fx.stop();

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
  if (server.arg("power") == "true") {
    server.send(200, "text/plain", LED_Switch->on ? "1" : "0");
    return;
  }

  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

  switches[LED_Switch->name]["state"] = LED_Switch->state();
  switches[LED_Switch->name]["Last On Time"] = LED_Switch->prettyOnTime;
  switches[LED_Switch->name]["Last Off Time"] = LED_Switch->prettyOffTime;
  uint8_t segment = *(ws2812fx.getActiveSegments());
  uint8_t mode = ws2812fx.getMode(segment); 
  switches[LED_Switch->name]["Mode"] = ws2812fx.getModeName(mode);
  switches[LED_Switch->name]["Color"] = ws2812fx.getColor(segment);
  switches[LED_Switch->name]["Speed"] = ws2812fx.getSpeed(segment);

  doc["debug"] = debug;

  char timeString[20];
  struct tm *timeinfo = localtime(&now);
  strftime (timeString,20,"%D %T",timeinfo);
  doc["time"] = timeString;

  size_t jsonDocSize = measureJsonPretty(doc);
  if (jsonDocSize > JSON_SIZE) {
    char msg[40];
    sprintf(msg, "ERROR: JSON message too long, %d", jsonDocSize);
    server.send(404, "text/plain", msg);
  } else {
    String httpResponse;
    serializeJsonPretty(doc, httpResponse);
    server.send(404, "text/plain", httpResponse);
  }
}

void handleHelp() {
  String helpMessage = "/help";
  
  helpMessage += "/debug?level=status\n";
  helpMessage += "/debug?level=[012]\n";
  helpMessage += "/status\n";
  helpMessage += "/status?power=true\n";
  helpMessage += "/status?segments=true\n";
  helpMessage += "\n";

  helpMessage += "/modeIterate?direction=up\n";
  helpMessage += "/modeIterate?direction=down\n";
  helpMessage += "/modeSetByIndex?index=[0-9]+\n";
  helpMessage += "\n";

  helpMessage += "/colorIterate\n";
  helpMessage += "/colorSetByName?name=(RED|GREEN|BLUE|WHITE|BLACK|YELLOW|CYAN|MAGENTA|PURPLE|ORANGE|PINK|GRAY)\n";
  helpMessage += "\n";

  helpMessage += "/power?state=on\n";
  helpMessage += "/power?state=off\n";

  server.send(200, "text/plain", helpMessage);
}

void handlePower() {
  if ( server.arg("state") ==  "on" ) {
    LED_Switch->switchOn();
    ws2812fx.start();
  } else if ( server.arg("state") ==  "off" ) {
    LED_Switch->switchOff();
    ws2812fx.stop();
  } else {
    String httpResponse = "unknown power state: ";
    httpResponse += server.arg("state");
    server.send(404, "text/plain", httpResponse);
    return;
  }
    
  syslog.logf(LOG_INFO, "Turned %s %s", LED_Switch->name, server.arg("state"));

  server.send(200, "text/plain");
}

void handleColorIterate() {
  uint8_t segment = *(ws2812fx.getActiveSegments());
  uint8_t colorIndex = ws2812fx.get_random_wheel_index(colorIndex);
  uint32_t color = ws2812fx.color_wheel(colorIndex);

  ws2812fx.setColor(segment, color);
  syslog.logf(LOG_INFO, "switching color to color_wheel index %d", colorIndex);

  server.send(200, "text/plain");
}

void handleModeSetByIndex() {
  if ( ! server.arg("index") ) {
    server.send(404, "text/plain", "Index for mode not specified");
    return;
  } 
  setModeByIndex((server.arg("index")).toInt());
  server.send(200, "text/plain");
}

void setModeByIndex(uint8_t newSegment) {

  uint8_t oldSegment = *(ws2812fx.getActiveSegments());
  uint8_t oldMode = ws2812fx.getMode(oldSegment); 

  uint8_t newMode = ws2812fx.getMode(newSegment); 
  ws2812fx.swapActiveSegment(oldSegment, newSegment);

  syslog.logf(LOG_INFO, "old seg %d; old mode %d; new seg %d; new mode: %d; new specified mode name: %s", 
      oldSegment, oldMode, newSegment, newMode, ws2812fx.getModeName(newMode));
  server.send(200, "text/plain");
}

void handleModeIterate() {
    if (server.arg("direction") == "up") switchMode(1);
    if (server.arg("direction") == "down") switchMode(-1);

    server.send(200, "text/plain");
}

uint8_t switchMode(int direction) {
    uint8_t oldSegment = *(ws2812fx.getActiveSegments());
    uint8_t oldMode = ws2812fx.getMode(oldSegment); 
    uint8_t newSegment;

    if ( direction == -1 && oldSegment == 0 ) {
      newSegment = ws2812fx.getNumSegments() - 1; 
    } else {
      newSegment = ( oldSegment + 1 * direction ) % ws2812fx.getNumSegments(); 
    }
    uint8_t newMode = ws2812fx.getMode(newSegment); 

    ws2812fx.swapActiveSegment(oldSegment, newSegment);

    syslog.logf(LOG_INFO, "old seg %d; old mode %d; new seg %d; new mode: %d; new mode name: %s", 
	oldSegment, oldMode, newSegment, newMode, ws2812fx.getModeName(newMode));

    char buffer[29];
    lcd->clear();
    sprintf(buffer, "%.18s", ws2812fx.getModeName(newSegment));
    lcd->print(buffer);
    lcd->setCursor(0, 0);

    return(newMode);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  if (encoderActivityTime + 100 < millis() && ledState == 1) {
    ws2812fx.resume();
    ledState = 0;
  }

  if (encoderActivityTime + 1 * 1000 < millis() && lcd->state) {
//    lcd->setBackLight(false);
    /*
    if (millis() % 300 == 0) {
      uint8_t segment = *(ws2812fx.getActiveSegments());
      uint8_t mode = ws2812fx.getMode(segment); 
      lcd->setCursor(0,0);
      lcd->scrollDisplayLeft();
      //lcd->print(ws2812fx.getModeName(mode));
    }
    */
  }

  // Do the encoder reading and processing
  if (versatile_encoder->ReadEncoder()) {
  }

  // if there's no one here, turn off the LEDs
  if (!motion->activity() && LED_Switch->on) { 
    LED_Switch->switchOff();
    syslog.log(LOG_INFO, "Olivia left cubby, turned her LED lights off");
  }

  time_t now = time(nullptr);

  ws2812fx.service();
}

// Implement your functions here accordingly to your needs
void handleRotate(int8_t rotation) {
  if ( ! lcd->state) {
    lcd->setBackLight(true);
  }

  encoderActivityTime = millis();
  ws2812fx.pause();
  ledState = 1;

  WS2812FX::Segment *segments = ws2812fx.getSegments();

  if (rotation < 0) {
    modeIndex = (modeIndex + 1) % ws2812fx.getNumSegments();
    if (debug) syslog.logf(LOG_INFO, "rotated right, modeIndex: %d", modeIndex);
  } else {
    if ( modeIndex == 0 ) modeIndex = ws2812fx.getNumSegments() - 1;
    else {
      modeIndex = (modeIndex - 1) % ws2812fx.getNumSegments();
    }
    if (debug) syslog.logf(LOG_INFO, "rotated left, modeIndex: %d", modeIndex);
  }

  lcd->clear();

  for (uint8_t row = 0; row < LCD_ROWS;  row++) { 
    lcd->setCursor(0, row);
    uint8_t index;
    if (modeIndex == 0 && row == 0) { 
      index = ws2812fx.getNumSegments() - 1;
    } else { 
      index = ( modeIndex + (row - 1) ) % ws2812fx.getNumSegments();
    }
    if (debug) {
      syslog.logf(LOG_INFO, "index: %d", index);
    }

    if (row == 1) lcd->print(">"); else lcd->print(" ");

    char buffer[29];
    char format[10];
    sprintf (format, "%%.%ds", LCD_COLS-2);
    sprintf(buffer, format, ws2812fx.getModeName(segments[index].mode));
    lcd->print(buffer);
  }
}

void handlePressRotate(int8_t rotation) {
  encoderActivityTime = millis();

  lcd->clear();
  if (rotation > 0) {
    led_color.nextColor();
    lcd->print(led_color.currentName());
  } else {
    led_color.previousColor();
    lcd->print(led_color.currentName());
  }
}

void handlePressRotateRelease() {
  uint8_t segment = *(ws2812fx.getActiveSegments());
  ws2812fx.setColor(segment, led_color.currentColor());
}

void handlePress() {
  if ( ! lcd->state) {
    lcd->setBackLight(true);
  }
  encoderActivityTime = millis();

  setModeByIndex(modeIndex);
  if (!LED_Switch->on) {
    LED_Switch->switchOn();
  }
}

void handleLongPress() {
  if ( ! lcd->state) {
    lcd->setBackLight(true);
  }
  encoderActivityTime = millis();

  if (LED_Switch->on) {
    LED_Switch->switchOff();
    syslog.log(LOG_INFO, "Turning off LEDs by longpress");
  }

  lcd->clear();
  lcd->print("Powered Off");
  lcd->setCursor(0, 1);
  lcd->print("LED Lightstrip");
}

