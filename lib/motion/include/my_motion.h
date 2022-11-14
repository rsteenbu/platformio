#ifndef MYMOTION_H
#define MYMOTION_H
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>
#include <Wire.h>
#include "Adafruit_MCP23X17.h"

// HC-SR501
// RCWL-0516 https://github.com/jdesbonnet/RCWL-0516

class MOTION {
  int pin;
  Adafruit_MCP23X17* mcp;
  bool i2cPins = false;
  time_t activityTime, now, prevTime = 0;

  public:
    bool motionState;
    char* name;
    time_t onTime = 0;
    time_t offTime = 0;
    int TIME_TO_HOLD = 15;

    MOTION (int a );
    MOTION (int a, Adafruit_MCP23X17* b);

    void setup(const char* a);
    bool activity();
    bool handle();
};
#endif
