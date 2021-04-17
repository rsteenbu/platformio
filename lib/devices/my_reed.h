#ifndef REED_H
#define REED_H
#include <Wire.h>
#include "Adafruit_MCP23017.h"

class ReedSwitch {
  int pin;
  Adafruit_MCP23017* mcp;
  bool i2cSwitch = false;

  public:
    //variables
    bool doorOpen;
    char* name;

    //constructors
    ReedSwitch () { }
    ReedSwitch (const char* a) { 
      name = new char[strlen(a)+1];
      strcpy(name,a);
    }

    void setup(int a, Adafruit_MCP23017* b) {
      pin = a;
      mcp = b;
      i2cSwitch = true;

      (*mcp).pinMode(pin, INPUT);
      (*mcp).pullUp(pin, HIGH);
    }

    void setup(int a) {
      pin = a;
      pinMode(pin, INPUT_PULLUP);
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

    int handle() {
      bool previousDoorOpen = doorOpen;
      
      if (i2cSwitch) {
	doorOpen = (*mcp).digitalRead(pin) ? true : false; // Check the door
      } else {
	doorOpen = digitalRead(pin) ? true : false; // Check the door
      }

      if (doorOpen && !previousDoorOpen) {
	//door closed
	return 1;
      }
      if (!doorOpen && previousDoorOpen) {
	//door opened
	return 2;
      }

      return 0;
    }
};

#endif

