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

    MOTION (int a ): pin(a) {}
    MOTION (int a, Adafruit_MCP23X17* b) {
      pin = a;
      mcp = b;
      i2cPins = true;
    }

   void setup(const char* a) {
      name = new char[strlen(a)+1];
      strcpy(name,a);

      i2cPins ? (*mcp).pinMode(pin, INPUT) : pinMode(pin, INPUT);
    }

    bool activity() {
      return motionState;
    }

    bool handle() {
      prevTime = now;
      now = time(nullptr);

      int activity;
      if ( i2cPins ) {
	activity = (*mcp).digitalRead(pin);
      } else {
	activity = digitalRead(pin);  // read input value from the motion sensor
      }

      if (activity == HIGH) {
        activityTime = now;
	if (motionState == false) {
	  onTime = now;
	  motionState = true;
	  return true;
        }
      }
      if (activity == LOW && motionState == true) {
        if (now > activityTime + TIME_TO_HOLD) {
	  offTime = now;
	  motionState = false;
	  return true;
        }
      }
      return false;
    }
};
#endif
