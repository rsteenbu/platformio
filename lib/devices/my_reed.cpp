#include "my_reed.h"

//constructors
ReedSwitch::ReedSwitch () { }
ReedSwitch::ReedSwitch(int a) {
  pin = a;
}
ReedSwitch::ReedSwitch(int a, Adafruit_MCP23X17* b) {
  pin = a;
  mcp = b;
  i2cPins = true;
}

void ReedSwitch::setup(const char* a) {
  name = new char[strlen(a)+1];
  strcpy(name,a);

  pinMode(pin, INPUT_PULLUP);
}

const char* ReedSwitch::state() {
  if (doorStatus == DOOR_OPEN) {
    return "opened";
  } else {
    return "closed";
  }
}

int ReedSwitch::status() {
  return doorStatus;
}

bool ReedSwitch::handle() {
  int previousDoorStatus = doorStatus;

  if (i2cPins) {
    doorStatus = (*mcp).digitalRead(pin); // Check the door
  } else {
    doorStatus = digitalRead(pin); // Check the door
  }

  return doorStatus != previousDoorStatus;
}
