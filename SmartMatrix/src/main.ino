#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoJson.h>
#include <Timezone.h> 
#include <WiFiEspAT.h>
#include <ArduinoHttpServer.h>

#include <my_ntp.h>
#include <teensy_relay.h>

#include <SD.h>

int debug = 0;
teensyRelay * Switch = new teensyRelay(22);

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

// SmartMatrix stuff
#define USE_ADAFRUIT_GFX_LAYERS
#include <MatrixHardware_Teensy4_ShieldV5.h> 
//#include <MatrixHardware_Teensy4_ShieldV4Adapter.h>
#include <SmartMatrix.h>
#include <Fonts/Org_01.h>



#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 256;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 128;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
//const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kPanelType = SM_PANELTYPE_HUB75_64ROW_MOD32SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
//SMARTMATRIX_ALLOCATE_GFX_MONO_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, kMatrixWidth*3, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
const uint8_t kMonoLayerOptions = (SM_GFX_MONO_OPTIONS_NONE);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kMonoLayerOptions);

// There are two Adafruit_GFX compatible layers, a Mono layer and an RGB layer, and this example sketch works with either (choose one):
#if 1
  const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
  SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
#else
  const uint8_t kMonoLayerOptions = (SM_GFX_MONO_OPTIONS_NONE);
  SMARTMATRIX_ALLOCATE_GFX_MONO_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kMonoLayerOptions);
  
  // these backwards compatible ALLOCATE macros also work:
  //SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kMonoLayerOptions);
  //SMARTMATRIX_ALLOCATE_INDEXED_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kMonoLayerOptions);
#endif

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// enable this to see the intermediate drawing steps, otherwise screen will only be updated at the end of each test (this slows the tests down considerably)
const bool swapAfterEveryDraw = false;

unsigned long testFillScreen() {
  unsigned long start = micros();
  backgroundLayer.fillScreen(BLACK);
  backgroundLayer.fillScreen(RED);
  backgroundLayer.fillScreen(GREEN);
  backgroundLayer.fillScreen(BLUE);
  backgroundLayer.fillScreen(BLACK);
  return micros() - start;
}

unsigned long testText() {
  backgroundLayer.fillScreen(BLACK);
  unsigned long start = micros();
  backgroundLayer.setFont(&Org_01);
  backgroundLayer.setCursor(0, 12);
//  backgroundLayer.setTextColor(WHITE);  backgroundLayer.setTextSize(1);
//  backgroundLayer.println("Hello Olivia!");
//  backgroundLayer.setTextColor(YELLOW); backgroundLayer.setTextSize(2);
//  backgroundLayer.println(1234.56);
//  backgroundLayer.setTextColor(RED);    backgroundLayer.setTextSize(3);
//  backgroundLayer.println(0xDEADBEEF, HEX);
//  backgroundLayer.println();
  backgroundLayer.setTextColor(GREEN);
  backgroundLayer.setTextSize(3);
  backgroundLayer.println("Groop");
  backgroundLayer.setTextSize(2);
  backgroundLayer.println("I implore thee,");
  backgroundLayer.setTextSize(1);
  backgroundLayer.println("my foonting turlingdromes.");
  backgroundLayer.println("And hooptiously drangle me");
  backgroundLayer.println("with crinkly bindlewurdles,");
  backgroundLayer.println("Or I will rend thee");
  backgroundLayer.println("in the gobberwarts");
  backgroundLayer.println("with my blurglecruncheon,");
  backgroundLayer.println("see if I don't!");
  return micros() - start;
}

unsigned long testLines(uint16_t color) {
  unsigned long start, t;
  int           x1, y1, x2, y2,
                w = backgroundLayer.width(),
                h = backgroundLayer.height();

  backgroundLayer.fillScreen(BLACK);

  x1 = y1 = 0;
  y2    = h - 1;
  start = micros();
  for(x2=0; x2<w; x2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  x2    = w - 1;
  for(y2=0; y2<h; y2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  t     = micros() - start; // fillScreen doesn't count against timing
  backgroundLayer.swapBuffers();

  backgroundLayer.fillScreen(BLACK);

  x1    = w - 1;
  y1    = 0;
  y2    = h - 1;
  start = micros();
  for(x2=0; x2<w; x2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  x2    = 0;
  for(y2=0; y2<h; y2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  t    += micros() - start;
  backgroundLayer.swapBuffers();

  backgroundLayer.fillScreen(BLACK);

  x1    = 0;
  y1    = h - 1;
  y2    = 0;
  start = micros();
  for(x2=0; x2<w; x2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  x2    = w - 1;
  for(y2=0; y2<h; y2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  t    += micros() - start;
  backgroundLayer.swapBuffers();

  backgroundLayer.fillScreen(BLACK);

  x1    = w - 1;
  y1    = h - 1;
  y2    = 0;
  start = micros();
  for(x2=0; x2<w; x2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  x2    = 0;
  for(y2=0; y2<h; y2+=6) {
    backgroundLayer.drawLine(x1, y1, x2, y2, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  t    += micros() - start;
  backgroundLayer.swapBuffers();

  return t;
}
unsigned long testFastLines(uint16_t color1, uint16_t color2) {
  unsigned long start;
  int           x, y, w = backgroundLayer.width(), h = backgroundLayer.height();

  backgroundLayer.fillScreen(BLACK);
  start = micros();
  for(y=0; y<h; y+=5) {
    backgroundLayer.drawFastHLine(0, y, w, color1);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }
  for(x=0; x<w; x+=5) {
    backgroundLayer.drawFastVLine(x, 0, h, color2);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }

  return micros() - start;
}

unsigned long testRects(uint16_t color) {
  unsigned long start;
  int           n, i, i2,
                cx = backgroundLayer.width()  / 2,
                cy = backgroundLayer.height() / 2;

  backgroundLayer.fillScreen(BLACK);
  n     = min(backgroundLayer.width(), backgroundLayer.height());
  start = micros();
  for(i=2; i<n; i+=6) {
    i2 = i / 2;
    backgroundLayer.drawRect(cx-i2, cy-i2, i, i, color);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }

  return micros() - start;
}

unsigned long testFilledRects(uint16_t color1, uint16_t color2) {
  unsigned long start, t = 0;
  int           n, i, i2,
                cx = backgroundLayer.width()  / 2 - 1,
                cy = backgroundLayer.height() / 2 - 1;

  backgroundLayer.fillScreen(BLACK);
  n = min(backgroundLayer.width(), backgroundLayer.height());
  for(i=n; i>0; i-=6) {
    i2    = i / 2;
    start = micros();
    backgroundLayer.fillRect(cx-i2, cy-i2, i, i, color1);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
    t    += micros() - start;
    // Outlines are not included in timing results
    backgroundLayer.drawRect(cx-i2, cy-i2, i, i, color2);
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }

  return t;
}

unsigned long testFilledCircles(uint8_t radius, uint16_t color) {
  unsigned long start;
  int x, y, w = backgroundLayer.width(), h = backgroundLayer.height(), r2 = radius * 2;

  backgroundLayer.fillScreen(BLACK);
  start = micros();
  for(x=radius; x<w; x+=r2) {
    for(y=radius; y<h; y+=r2) {
      backgroundLayer.fillCircle(x, y, radius, color);
      if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
    }
  }

  return micros() - start;
}

unsigned long testCircles(uint8_t radius, uint16_t color) {
  unsigned long start;
  int           x, y, r2 = radius * 2,
                w = backgroundLayer.width()  + radius,
                h = backgroundLayer.height() + radius;

  // Screen is not cleared for this one -- this is
  // intentional and does not affect the reported time.
  start = micros();
  for(x=0; x<w; x+=r2) {
    for(y=0; y<h; y+=r2) {
      backgroundLayer.drawCircle(x, y, radius, color);
      if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
    }
  }

  return micros() - start;
}

unsigned long testTriangles() {
  unsigned long start;
  int           n, i, cx = backgroundLayer.width()  / 2 - 1,
                      cy = backgroundLayer.height() / 2 - 1;

  backgroundLayer.fillScreen(BLACK);
  n     = min(cx, cy);
  start = micros();
  for(i=0; i<n; i+=5) {
    backgroundLayer.drawTriangle(
      cx    , cy - i, // peak
      cx - i, cy + i, // bottom left
      cx + i, cy + i, // bottom right
      backgroundLayer.color565(0, 0, i));
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }

  return micros() - start;
}

unsigned long testFilledTriangles() {
  unsigned long start, t = 0;
  int           i, cx = backgroundLayer.width()  / 2 - 1,
                   cy = backgroundLayer.height() / 2 - 1;

  backgroundLayer.fillScreen(BLACK);
  start = micros();
  for(i=min(cx,cy); i>10; i-=5) {
    start = micros();
    backgroundLayer.fillTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
      backgroundLayer.color565(0, i, i));
    t += micros() - start;
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
    backgroundLayer.drawTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
      backgroundLayer.color565(i, i, 0));
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }

  return t;
}

unsigned long testRoundRects() {
  unsigned long start;
  int           w, i, i2,
                cx = backgroundLayer.width()  / 2 - 1,
                cy = backgroundLayer.height() / 2 - 1;

  backgroundLayer.fillScreen(BLACK);
  w     = min(backgroundLayer.width(), backgroundLayer.height());
  start = micros();
  for(i=0; i<w; i+=6) {
    i2 = i / 2;
    backgroundLayer.drawRoundRect(cx-i2, cy-i2, i, i, i/8, backgroundLayer.color565(i, 0, 0));
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }

  return micros() - start;
}

unsigned long testFilledRoundRects() {
  unsigned long start;
  int           i, i2,
                cx = backgroundLayer.width()  / 2 - 1,
                cy = backgroundLayer.height() / 2 - 1;

  backgroundLayer.fillScreen(BLACK);
  start = micros();
  for(i=min(backgroundLayer.width(), backgroundLayer.height()); i>20; i-=6) {
    i2 = i / 2;
    backgroundLayer.fillRoundRect(cx-i2, cy-i2, i, i, i/8, backgroundLayer.color565(0, i, 0));
    if(swapAfterEveryDraw) backgroundLayer.swapBuffers();
  }

  return micros() - start;
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial1.begin(115200);
  Switch->setup("MySwitch");

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
  Serial.print("Syslog server set to: ");
  Serial.println(SYSLOG_SERVER);
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

  // Initialize the SD Card interface
  /*
  bool ok;
  ok = SD.sdfs.begin(SdioConfig(FIFO_SDIO));
  if (!ok) {
    Serial.println("SD.sdfs initialization failed!");
    return;
  } else {
    Serial.println("SD.sdfs initialization done.");
    Serial.println();
  }
*/

  //SmartMatrix stuff
  matrix.addLayer(&backgroundLayer); 
  matrix.addLayer(&scrollingLayer); 
  matrix.begin();

  testFilledTriangles();

  backgroundLayer.swapBuffers();
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

void handleStatus(WiFiClient& client) {
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");
  JsonObject sensors = doc.createNestedObject("sensors");

  switches[Switch->name]["state"] = Switch->state();
  switches[Switch->name]["Last On Time"] = Switch->prettyOnTime;
  switches[Switch->name]["Last Off Time"] = Switch->prettyOffTime;
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

void handleSwitch(ArduinoHttpServer::StreamHttpRequest<1024>& httpRequest, WiFiClient& client) {
  if (httpRequest.getResource()[1] == "status") {
    Serial.println("Returning switch status");
    ArduinoHttpServer::StreamHttpReply httpReply(client, "text/html");
    String msg = Switch->state();
    httpReply.send(msg);
  } else if (httpRequest.getResource()[1] == "switchOn") {
    Switch->switchOn();
    Serial.println("Turned switch on");
    ArduinoHttpServer::StreamHttpReply httpReply(client, "text/html");
    httpReply.send("");
    syslog.logf(LOG_INFO, "Turned %s on", Switch->name);
  } else if (httpRequest.getResource()[1] == "switchOff") {
    Switch->switchOff();
    Serial.println("Turned switch off");
    ArduinoHttpServer::StreamHttpReply httpReply(client, "text/html");
    httpReply.send("");
    syslog.logf(LOG_INFO, "Turned %s off", Switch->name);
  } else {
    String msg = "ERROR: uknown switch command: ";
    msg += httpRequest.getResource()[1];
    msg += "\n\n";
    ArduinoHttpServer::StreamHttpErrorReply httpReply(client, "text/html", "400");
    httpReply.send(msg);
  }
}

void loop() {
  //#define GREEN   0x07E0
  scrollingLayer.setColor({0x00, 0x00, 0xff});
  scrollingLayer.setOffsetFromTop(1);
  scrollingLayer.setMode(wrapForward);
  scrollingLayer.setSpeed(80);
  scrollingLayer.setFont(font6x10);
  scrollingLayer.start("Ever since I was a young boy, I played the silver ball", 1);
  while (scrollingLayer.getStatus());

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
        } else if (httpRequest.getResource()[0] == "switch") {
          handleSwitch(httpRequest, client);
        } else {
          handleNotFound(client);
        }
     }
     client.stop();
  }
}

