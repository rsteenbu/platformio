#ifndef REED_H
#define REED_H
#include <Wire.h>
#include "Adafruit_MCP23X17.h"

class ReedSwitch {
  int pin;
  Adafruit_MCP23X17* mcp;
  bool i2cPins = false;
  const int DOOR_OPEN = 1;
  const int DOOR_CLOSED = 1;

  public:
    //variables
    int doorStatus;
    char* name;

    //constructors
    ReedSwitch ();
    ReedSwitch(int a);
    ReedSwitch(int a, Adafruit_MCP23X17* b);

    void setup(const char* a);
    const char* state();
    int status();
    bool handle();
};

#endif

