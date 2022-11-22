#include "teensy_relay.h"

void teensyRelay::internalOn() {
  if (!isOn) {
    digitalWrite(pin, onVal);
    isOn = true;
    //sprintf(prettyOnTime, "%02d/%02d/%04d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
  }
}

void teensyRelay::internalOff() {
  if (isOn) {
    digitalWrite(pin, offVal);
    isOn = false;
    //sprintf(prettyOffTime, "%02d/%02d/%04d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
  }
}

    //constructors
teensyRelay::teensyRelay (int a): pin(a) {}

void teensyRelay::setBackwards() {
  onVal = LOW;
  offVal = HIGH;
}

int teensyRelay::status() {
  if (isOn) {
    return 1;
  } else {
    return 0;
  }
}

void teensyRelay::setup(const char* a) {
  name = new char[strlen(a)+1];
  strcpy(name,a);
  this->setup();
}

void teensyRelay::setup() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, offVal); // start off

  // iternate through one on off cycle to work the kinks out
  //internalOn(); internalOff();

  onTime = offTime = Teensy3Clock.get();
  strcpy(prettyOnTime,"None");
  strcpy(prettyOffTime,"None");
}

void teensyRelay::switchOn() {
  internalOn();
}

void teensyRelay::switchOff() {
  internalOff();
}

const char* teensyRelay::state() {
  if (isOn) {
    return "on";
  } else { 
    return "off";
  }
}
