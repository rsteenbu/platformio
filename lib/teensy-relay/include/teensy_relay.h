#ifndef TEENSY_RELAY_H
#define TEENSY_RELAY_H

#include <Timezone.h> 
#include <TimeLib.h>

class teensyRelay {
  int onVal = HIGH;
  int offVal = LOW;
  bool i2cPins = false;

  protected:
    int pin;
    void internalOn();
    void internalOff();

  public:
    bool isOn = false;
    time_t onTime = 0;
    time_t offTime = 0;
    char prettyOffTime[18];
    char prettyOnTime[18];
    char* name;

    //constructors
    teensyRelay (int a);

    void setBackwards();
    int status();
    void setup(const char* a);
    void setup();
    void switchOn();
    void switchOff();
    const char* state();
};

#endif

