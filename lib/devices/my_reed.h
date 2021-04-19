#ifndef REED_H
#define REED_H
#include <Wire.h>
#include "Adafruit_MCP23017.h"

class ReedSwitch {
  int pin;
  Adafruit_MCP23017* mcp;
  bool i2cPins = false;

  public:
    //variables
    bool doorOpen;
    char* name;

    //constructors
    ReedSwitch () { }
    ReedSwitch(int a) {
      pin = a;
    }
    ReedSwitch(int a, Adafruit_MCP23017* b) {
      pin = a;
      mcp = b;
      i2cPins = true;
    }

    void setup(const char* a) {
      name = new char[strlen(a)+1];
      strcpy(name,a);

      if (i2cPins) {
	(*mcp).pinMode(pin, INPUT);
	(*mcp).pullUp(pin, HIGH);
      } else {
	pinMode(pin, INPUT_PULLUP);
      }
    }

    const char* state() {
      if (doorOpen) {
	return "open";
      } else {
	return "closed";
      }
    }

    int status() {
      if (doorOpen) {
	return 1;
      } else {
	return 0;
      }
    }

    bool handle() {
      bool previousDoorOpen = doorOpen;
      
      if (i2cPins) {
	doorOpen = (*mcp).digitalRead(pin) ? true : false; // Check the door
      } else {
	doorOpen = digitalRead(pin) ? true : false; // Check the door
      }

      if (doorOpen && !previousDoorOpen) {
	//door closed
	return true;
      }
      if (!doorOpen && previousDoorOpen) {
	//door opened
	return true;
      }

      return 0;
    }
};

#endif

