#ifndef RELAY_H
#define RELAY_H

class Relay {
  int pin;
  bool backwards;
  int onVal, offVal;

  public:
    Relay (int a): pin(a) {
        onVal = HIGH;
        offVal = LOW;
    }
    Relay (int a, bool b): pin(a), backwards(b) {
      if (backwards) {
        onVal = LOW;
        offVal = HIGH;
      } else {
        onVal = HIGH;
        offVal = LOW;
      }
    }

    bool on = false;
    void setup() {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, offVal); // start off
    }
    void switchOn() {
      if (!on) {
        digitalWrite(pin, onVal);
        on = true;
      }
    }
    void switchOff() {
      if (on) {
        digitalWrite(pin, offVal);
        on = false;
      }
    }
    const char* state() {
      if (on) {
        return "on";
      } else {
        return "off";
      }
    }
};

#endif

