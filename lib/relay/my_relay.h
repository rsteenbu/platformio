#ifndef RELAY_H
#define RELAY_H
#define MYTZ TZ_America_Los_Angeles
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>
#include <Array.h>

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

    void setBackwards(bool a) {
      backwards = a;
    }
    void setScheduleOverride(bool a) {
      scheduleOverride = a;
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
    int runTime = 0;
    int startHour;
    int startMinute;
    Array<int,7> runDays;

    TimerRelay(int a): Relay(a) {}

    void setRuntime(int a) {
      runTime = a;
    }
    
    void setStartTime(int a, int b) {
      startHour = a;
      startMinute = b;
    }

   void setEveryOtherDayOn() {
     for ( int n=0 ; n<7 ; n++ ) {
       if (n % 2 == 0) {
	 runDays[n] = 1;
       } else {
	 runDays[n] = 0;
       }
     }
   }

   void setEveryDayOn() {
     for ( int n=0 ; n<7 ; n++ ) {
       runDays[n] = 1;
     }
   }

   void setEveryDayOff() {
     for ( int n=0 ; n<7 ; n++ ) {
       runDays[n] = 0;
     }
   }

   void setDaysFromArray(Array<int,7> & array) {
     for ( int n=0 ; n<7 ; n++ ) {
       runDays[n] = array[n];
     }
   } 

   void setSpecificDayOn(int n) {
     runDays[n] = 1;
   }

   int checkToRun(int weekDay) { 
     return runDays[weekDay];
   } 

   int handle() {
     time_t now = time(nullptr);
     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;
     int currMinute = timeinfo->tm_min;
     int weekDay = timeinfo->tm_wday;

     int currMinuteOfDay = (currHour * 60) + currMinute;
     int startMinuteOfDay = (startHour * 60) + startMinute;

     if ( runTime == 0 ) {
       return 0;
     }

     // if we're not on, turn it on if it's the right day and time
     if (!on) {
       if ( runDays[weekDay] && currMinuteOfDay == startMinuteOfDay )
       {
	 switchOn();
	 return 1;
       } else {
	 return 0;
       }
     // if we're on, turn it off if it's been more than than the time to run
     } else {
       if ( now >= onTime + (runTime * 60)  ) {
	 switchOff();
	 return 2;
       }
     }
     return 0;
   }

};


#endif

