#ifndef TEENSYRELAY_H
#define TEENSYRELAY_H

class teensyRelay {
  int onVal = HIGH;
  int offVal = LOW;

  protected:
    int pin;

  public:
    //variables
    bool on = false;
    char* name;

    //constructors
    teensyRelay (int a): pin(a) {}

    int status() {
      if (on) {
	return 1;
      } else {
	return 0;
      }
    }
    void setup(const char* a) {
      name = new char[strlen(a)+1];
      strcpy(name,a);

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
