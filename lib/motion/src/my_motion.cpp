#include "my_motion.h"

// HC-SR501
// RCWL-0516 https://github.com/jdesbonnet/RCWL-0516

MOTION::MOTION (int a ): pin(a) {}
MOTION::MOTION (int a, Adafruit_MCP23X17* b) {
  pin = a;
  mcp = b;
  i2cPins = true;
}

void MOTION::setup(const char* a) {
  name = new char[strlen(a)+1];
  strcpy(name,a);

  i2cPins ? (*mcp).pinMode(pin, INPUT) : pinMode(pin, INPUT);
}

bool MOTION::activity() {
  return motionState;
}

bool MOTION::handle() {
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
