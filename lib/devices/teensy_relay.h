#ifndef TEENSYRELAY_H
#define TEENSYRELAY_H

#include <TimeLib.h>

class teensyRelay {
  int onVal = HIGH;
  int offVal = LOW;

  private:
    void setPrettyTime(char prettyTime[18], time_t time ) {
      tmElements_t timeinfo;

      breakTime(time, timeinfo);
      sprintf(prettyTime, "%02d/%02d/%04d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
    }

  protected:
    int pin;

  public:
    //variables
    bool on = false;
    time_t onTime = 0;
    time_t offTime = 0;
    char* name;
    char prettyOnTime[20] = "None";
    char prettyOffTime[20] = "None";

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
	onTime = now();
	setPrettyTime(prettyOnTime, onTime);
        digitalWrite(pin, onVal);
        on = true;
      }
    }
    void switchOff() {
      if (on) {
	offTime = now();
	setPrettyTime(prettyOffTime, offTime);
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
