#ifndef RELAY_H
#define RELAY_H
#define MYTZ TZ_America_Los_Angeles
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

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
    bool scheduleOverride = false;
    time_t onTime = 0;
    time_t offTime = 0;

    void setScheduleOverride(bool s) {
      scheduleOverride = s;
    }
    bool getScheduleOverride() {
      return scheduleOverride;
    }
    void setup() {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, offVal); // start off
      configTime(MYTZ, "pool.ntp.org");
      offTime = time(nullptr);
    }
    void switchOn() {
      if (!on) {
        digitalWrite(pin, onVal);
        on = true;
	onTime = time(nullptr);
      }
    }
    void switchOff() {
      if (on) {
        digitalWrite(pin, offVal);
        on = false;
	offTime = time(nullptr);
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

class TimerRelay: public Relay {
  public:
    int runTime;
    TimerRelay(int a): Relay(a) {}

    void setRuntime(int a) {
      runTime = a;
    }
};


#endif

