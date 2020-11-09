#ifndef RELAY_H
#define RELAY_H

class Relay {
  int pin;

  public:
    Relay (int a): pin(a) {}
    bool on = false;
    void setup() {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW); // start off
    }
    void switchOn() {
      if (!on) {
        digitalWrite(pin, HIGH);
        on = true;
      }
    }
    void switchOff() {
      if (on) {
        digitalWrite(pin, LOW);
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

//Relay::Relay(int) {};

#endif

