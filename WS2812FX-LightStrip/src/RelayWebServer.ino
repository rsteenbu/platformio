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

#include <EEPROM.h>
#define EEPROM_SIZE 4

#include <Wire.h>

#ifndef NOLCD
#include <my_motion.h>
MOTION * motion = new MOTION(MOTION_PIN); 
#include <my_lcd.h>
LCD * lcd = new LCD();
#include <my_relay.h>
Relay * LED_Switch = new Relay(RELAY_PIN);
#include <Versatile_RotaryEncoder.h>
// Create a global pointer for the encoder object
Versatile_RotaryEncoder *versatile_encoder;

#endif

#include <ws2812fx.h>

WS2812FX ws2812fx_p1 = WS2812FX(1, LED_PIN_P1, NEO_GRB + NEO_KHZ800, 72);
#ifdef LED_PIN_P2
WS2812FX ws2812fx_p2 = WS2812FX(1, LED_PIN_P2, NEO_GRB + NEO_KHZ800, 72);
#endif
#ifdef LED_PIN_P3
WS2812FX ws2812fx_p3 = WS2812FX(1, LED_PIN_P3, NEO_GRB + NEO_KHZ800, 72);
#endif
#ifdef LED_PIN_P4
WS2812FX ws2812fx_p4 = WS2812FX(1, LED_PIN_P4, NEO_GRB + NEO_KHZ800, 72);
#endif
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN_P1, NEO_GRB + NEO_KHZ800, 72);


int colorIndex = 0;
int modeIndex = 0;
unsigned long encoderActivityTime;
int ledState = 0; // 0 runnning; 1 paused

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


using namespace std;

class myColors {
  public:
    enum values { red = RED, green = GREEN, blue = BLUE, white = WHITE, yellow = YELLOW, cyan = CYAN, magenta = MAGENTA, purple = PURPLE, orange = ORANGE, pink = PINK, gray = GRAY, ultrawhite = ULTRAWHITE };
    int currentColorIndex;
    //const uint32_t colors[3] = {RED, GREEN, BLUE};

    values index[12] = {values::red, values::green, values::blue, values::white, 
                        values::yellow, values::cyan, values::magenta, values::purple, 
                        values::orange, values::pink, values::gray, values::ultrawhite}; 
    //constructors
    myColors();
    

    const char* currentName() {
      return nameForIndex(currentColorIndex);
    }

    const char* nameForIndex(int i) {
      switch(index[i]) {
        case values::red        : return "red";        break;
        case values::green      : return "green";      break;
        case values::blue       : return "blue";       break;
        case values::white      : return "white";      break;
        case values::yellow     : return "yellow";     break;
        case values::cyan       : return "cyan";       break;
        case values::magenta    : return "magenta";    break;
        case values::purple     : return "purple";     break;
        case values::orange     : return "orange";     break;
        case values::pink       : return "pink";       break;
        case values::gray       : return "gray";       break;
        case values::ultrawhite : return "ultrawhite"; break;
      }
      return "undefined";
    }

    uint32_t currentColor() {
      return index[currentColorIndex];
    }
    //cycle through the colors, with the rotary dial.
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

    bool setColorByName(const char *a) {
      for (int i = 0; i < 12; i++) {
	if (strcmp(nameForIndex(i), a) == 0) {
	  currentColorIndex = i;
	  return true;
	}
      }
      return false;
    }
};

myColors::myColors () {
  currentColorIndex = 0;
}

// the three colors used by ws2812fx
//const uint32_t colors[1] = {PURPLE};
const uint32_t colors[3] = {RED, GREEN, BLUE};

// all the colors available to use and how to use them
myColors led_colors[3];

void setup() {
  Serial.begin(115200);
  Serial.println("Booting up");

  EEPROM.begin(EEPROM_SIZE);

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
#ifndef NOLCD
  Wire.begin(21, 22);
  if (!lcd->begin(LCD_COLS, LCD_ROWS))  {
    syslog.log(LOG_INFO, "LCD Initialization failed");
  }
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

  LED_Switch->setup("ledswitch");
#endif


#ifdef ESP32
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
#else
  configTime(MYTZ, "pool.ntp.org");
#endif

  server.on("/help", handleHelp);
  server.on("/debug", handleDebug);
  server.on("/status", handleStatus);
  server.on("/modeIterate", handleModeIterate);
  server.on("/modeSetByIndex", handleModeSetByIndex);
  server.on("/listModes", handleListModes);
  server.on("/colorIterate", handleSetColorByIterate);
  server.on("/colorSetByName", handleSetColorByName);
  server.on("/setBrightness", handlesetBrightness);
  server.on("/power", handlePower);
  server.begin();



  // initialize the virtual strip as you would any normal ws2812fx instance
  ws2812fx.init();
  ws2812fx.setBrightness(32);
  ws2812fx.setSegment(0, 0, ws2812fx.getLength()-1, FX_MODE_STATIC, RED, 2000, NO_OPTIONS);

  // init the physical strip's GPIOs and reassign their pixel data
  // pointer to use the virtual strip's pixel data.
  ws2812fx_p1.init();
  ws2812fx_p1.setPixels(LED_COUNT_P1, ws2812fx.getPixels());

#ifdef LED_PIN_P2
  ws2812fx_p2.init();
  ws2812fx_p2.setPixels(LED_COUNT_P2, ws2812fx.getPixels() + (LED_COUNT_P1 * ws2812fx.getNumBytesPerPixel()));
#endif

#ifdef LED_PIN_P3
  ws2812fx_p3.init();
  ws2812fx_p3.setPixels(LED_COUNT_P3, ws2812fx.getPixels() + ((LED_COUNT_P1+LED_COUNT_P2) * ws2812fx.getNumBytesPerPixel()));
#endif

#ifdef LED_PIN_P4
  ws2812fx_p4.init();
  ws2812fx_p4.setPixels(LED_COUNT_P4, ws2812fx.getPixels() + ((LED_COUNT_P1+LED_COUNT_P2+LED_COUNT_P4) * ws2812fx.getNumBytesPerPixel()));
#endif
  
  
  // config a custom show() function for the virtual strip, so pixel
  // data gets sent to the physical strips's LEDs instead
  ws2812fx.setCustomShow(myCustomShow);

  ws2812fx.stop();
}

// update the physical strips's LEDs
void myCustomShow(void) {
  ws2812fx_p1.show();
#ifdef LED_PIN_P2
  ws2812fx_p2.show();
#endif
#ifdef LED_PIN_P3
  ws2812fx_p3.show();
#endif
#ifdef LED_PIN_P4
  ws2812fx_p4.show();
#endif
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
#ifndef NOLCD
    server.send(200, "text/plain", LED_Switch->on ? "1" : "0");
#else
    server.send(200, "text/plain", "0");
#endif
    return;
  }

  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

#ifndef NOLCD
  switches["ledstrip"]["state"] = LED_Switch->state();
  switches["ledstrip"]["Last On Time"] = LED_Switch->prettyOnTime;
  switches["ledstrip"]["Last Off Time"] = LED_Switch->prettyOffTime;
#endif
  uint8_t segment = *(ws2812fx.getActiveSegments());
  uint8_t mode = ws2812fx.getMode(segment); 
  switches["ledstrip"]["Mode"] = ws2812fx.getModeName(mode);
  switches["ledstrip"]["ModeNumber"] = ws2812fx.getMode(mode);
  switches["ledstrip"]["Color0"] = led_colors[0].currentName();
  switches["ledstrip"]["Color1"] = led_colors[1].currentName();
  switches["ledstrip"]["Color2"] = led_colors[2].currentName();
  switches["ledstrip"]["Speed"] = ws2812fx.getSpeed(segment);
  switches["ledstrip"]["brightness"] = ws2812fx.getBrightness();
  switches["ledstrip"]["modecount"] = MODE_COUNT;

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
  helpMessage += "/listModes\n";
  helpMessage += "\n";

  helpMessage += "/setBrightness?brightness=(0-255)\n";

  helpMessage += "/colorIterate?number=[012]\n";
  helpMessage += "/colorSetByName?number=[012]?name=(red|green|blue|white|black|yellow|cyan|magenta|purple|orange|pink|gray)\n";
  helpMessage += "\n";

  helpMessage += "/power?state=on\n";
  helpMessage += "/power?state=off\n";

  server.send(200, "text/plain", helpMessage);
}

void handleSetColorByIterate() {
  bool byIteration = true;
  setColor(byIteration);
}

void handleSetColorByName() {
  bool byIteration = false;
  setColor(byIteration);
}

void setColor(bool byIteration) {
  uint8_t segment = *(ws2812fx.getActiveSegments());

  char msg[100];
  int colorNumber = server.arg("number").toInt();
  if (colorNumber != 0 && colorNumber != 1 && colorNumber!= 2) {
      sprintf(msg, "ERROR: %s number not correct for number arg", server.arg("number"));
      server.send(404, "text/plain", msg);
      return;
  }
 
  if (byIteration) {
    led_colors[colorNumber].nextColor();
  } else {
    char *color = new char[12];
    strcpy(color,server.arg("color").c_str());

    if (!led_colors[colorNumber].setColorByName(color)) {
      sprintf(msg, "ERROR: Color %s not found", server.arg("color"));
      server.send(404, "text/plain", msg);
      return;
    }
  }

  uint32_t colors[] = {led_colors[0].currentColor(), led_colors[1].currentColor(), led_colors[2].currentColor()};
  ws2812fx.setColors(segment, colors) ;

  syslog.logf(LOG_INFO, "switching colors to %s:%s:%s", led_colors[0].currentName(), led_colors[1].currentName(), led_colors[2].currentName());

  server.send(200, "text/plain");
}

  
void handleListModes() {
  char PROGMEM msg[1500] = ""; 
  char indexString[10]; 

  for (int i = 0; i < MODE_COUNT; i++) {
    sprintf(indexString, "%d: ", i);
    strcat_P(msg, indexString);
    strcat_P(msg, (char *)pgm_read_ptr(&(_modes[i])));  
    strcat_P (msg, "\n");
  }
  server.send(200, "text/plain", msg);
}

void handleModeSetByIndex() {
  if ( ! server.arg("index") ) {
    server.send(404, "text/plain", "Index for mode not specified");
    return;
  } 
  setModeByIndex((server.arg("index")).toInt());
  server.send(200, "text/plain");
}

void setModeByIndex(uint8_t newMode) {

  uint8_t segment = *(ws2812fx.getActiveSegments());
  uint8_t mode = ws2812fx.getMode(segment); 

  ws2812fx.setMode(newMode);

  syslog.logf(LOG_INFO, "old mode %d; new mode: %d; new specified mode name: %s", 
      mode, newMode, ws2812fx.getModeName(newMode));
}

void handleModeIterate() {
    if (server.arg("direction") == "up") switchMode(1);
    if (server.arg("direction") == "down") switchMode(-1);

    server.send(200, "text/plain");
}

void handlesetBrightness() {
  if ( ! server.arg("brightness")) {
    server.send(404, "text/plain", "brightness not set");
  }

  int brightNess = server.arg("brightness").toInt();

  if ( brightNess < 0 || brightNess > 255 ) {
    server.send(404, "text/plain", "brightness is not in range of 0..255");
  }

  ws2812fx.setBrightness(brightNess);
  server.send(200, "text/plain");

}

void handlePower() {
  if ( server.arg("state") ==  "on" ) {
#ifndef NOLCD
    LED_Switch->switchOn();
#endif
    ws2812fx.start();
  } else if ( server.arg("state") ==  "off" ) {
#ifndef NOLCD
    LED_Switch->switchOff();
#endif
    ws2812fx.stop();
  } else {
    String httpResponse = "unknown power state: ";
    httpResponse += server.arg("state");
    server.send(404, "text/plain", httpResponse);
    return;
  }
  syslog.logf(LOG_INFO, "Turned ledstrip %s", server.arg("state"));

  server.send(200, "text/plain");
}

uint8_t switchMode(int direction) {
    uint8_t segment = *(ws2812fx.getActiveSegments());
    uint8_t mode = ws2812fx.getMode(segment); 
    uint8_t newMode;

    if ( direction == -1 && mode == 0 ) {
      newMode = MODE_COUNT - 1; 
    } else {
      newMode = ( mode + 1 * direction ) % MODE_COUNT; 
    }

    ws2812fx.setMode(newMode);

    syslog.logf(LOG_INFO, "old mode %d; new mode: %d; new mode name: %s", 
	mode, newMode, ws2812fx.getModeName(newMode));

#ifndef NOLCD
    char buffer[29];
    lcd->clear();
    sprintf(buffer, "%.18s", ws2812fx.getModeName(newMode));
    lcd->print(buffer);
    lcd->setCursor(0, 0);
#endif

    return(newMode);
}

uint8_t switchModeWithSegment(int direction) {
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

#ifndef NOLCD
    char buffer[29];
    lcd->clear();
    sprintf(buffer, "%.18s", ws2812fx.getModeName(newSegment));
    lcd->print(buffer);
    lcd->setCursor(0, 0);
#endif

    return(newMode);
}



bool activity = false;
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  time_t now = time(nullptr);
  ws2812fx.service();

#ifndef NOLCD
  if (encoderActivityTime + 100 < millis() && ledState == 1) {
    ws2812fx.resume();
    ledState = 0;
  }

//    lcd->setBackLight(false);
  if (encoderActivityTime + 5 * 1000 < millis() && lcd->state) {

      uint8_t segment = *(ws2812fx.getActiveSegments());
      uint8_t mode = ws2812fx.getMode(segment); 

      if (activity) {
	activity = false;
	homeDisplay();
      }
  } else {
    activity = true;
  }

  // Do the encoder reading and processing
  if (versatile_encoder->ReadEncoder()) {
  }

  // if there's no one here, turn off the LEDs
  if (!motion->activity() && LED_Switch->on) { 
    LED_Switch->switchOff();
    syslog.log(LOG_INFO, "Olivia left cubby, turned her LED lights off");
  }
#endif

}

#ifndef NOLCD
void homeDisplay() {
  uint8_t segment = *(ws2812fx.getActiveSegments());
  uint8_t mode = ws2812fx.getMode(segment); 
  lcd->setCursor(0,0);
  lcd->clear();
  lcd->print(ws2812fx.getModeName(mode));
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

ws2812fxColorIndex = 0;
selectingColor = false;
void handlePressRotate(int8_t rotation) {
  encoderActivityTime = millis();
  selectingColor true;

  if (rotation > 0) {
    ws2812fxColorIndex = (ws2812fxColorIndex + 1) % 3;
  } else {
    if (ws2812fxColorIndex == 0) ws2812fxColorIndex = 2;
    else ws2812fxColorIndex = (ws2812fxColorIndex - 1) % 3;
  }

  lcd->clear();
  lcd->print("LED Color ");
  lcd->print(ws2812fxColorIndex+1);
  lcd->print(":");
}

void handlePressRotate1(int8_t rotation) {
  encoderActivityTime = millis();

  lcd->setCursor(0, 1);
  lcd->print("                    ");
  lcd->setCursor(0, 1);
  if (rotation > 0) {
    led_colors[0].nextColor();
  } else {
    led_colors[0].previousColor();
  }
  lcd->print(led_colors[0].currentName());
}

void handlePressRotateRelease1() {
  uint8_t segment = *(ws2812fx.getActiveSegments());
  ws2812fx.setColor(segment, led_colors[0].currentColor());
  homeDisplay();
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
#endif

