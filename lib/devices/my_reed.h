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
    ReedSwitch () { }
    ReedSwitch(int a) {
      pin = a;
    }
    ReedSwitch(int a, Adafruit_MCP23X17* b) {
      pin = a;
      mcp = b;
      i2cPins = true;
    }

    void setup(const char* a) {
      name = new char[strlen(a)+1];
      strcpy(name,a);

      pinMode(pin, INPUT_PULLUP);
    }

    const char* state() {
      if (doorStatus == DOOR_OPEN) {
	return "opened";
      } else {
	return "closed";
      }
    }

    int status() {
      return doorStatus;
    }

    bool handle() {
      int previousDoorStatus = doorStatus;
      
      if (i2cPins) {
	doorStatus = (*mcp).digitalRead(pin); // Check the door
      } else {
	doorStatus = digitalRead(pin); // Check the door
      }

      return doorStatus != previousDoorStatus;
    }
};

#endif

