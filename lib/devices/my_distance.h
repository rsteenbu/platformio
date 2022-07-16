#ifndef MYDISTANCE_H
#define MYDISTANCE_H
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>
#include <Wire.h>
#include "Adafruit_MCP23X17.h"

//HC-SR04
 
// Can't use MCP b/c it doesn't support pulseIn
class DISTANCE {
  int echoPin;
  int trigPin;
  Adafruit_MCP23X17* mcp;
  bool i2cPins = false;
  time_t now, prevTime = 0;

  public:
    long inches;
    char* name;

    //constructor
    DISTANCE (int a, int b );

    void setup(const char* a);
    void handle();
};
#endif
