#include "wifi_config.h"

#include <ArduinoJson.h>
#include <SD.h>

// SmartMatrix stuff
#define USE_ADAFRUIT_GFX_LAYERS
#include <MatrixHardware_Teensy4_ShieldV5.h> 
//#include <MatrixHardware_Teensy4_ShieldV4Adapter.h>
#include <SmartMatrix.h>
#include <Fonts/Org_01.h>

#define COLOR_DEPTH 24
const uint16_t kMatrixWidth = 256;
const uint16_t kMatrixHeight = 128;
const uint8_t kRefreshDepth = 36;
const uint8_t kDmaBufferRows = 4;
const uint8_t kPanelType = SM_PANELTYPE_HUB75_64ROW_MOD32SCAN;
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
const uint8_t kMonoLayerOptions = (SM_GFX_MONO_OPTIONS_NONE);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kMonoLayerOptions);

const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

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

  backgroundLayer.fillScreen(WHITE);
  //testFilledTriangles();

  backgroundLayer.swapBuffers();
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

  backgroundLayer.fillScreen(GREEN);

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
        } else {
          handleNotFound(client);
        }
     }
     client.stop();
  }
}

