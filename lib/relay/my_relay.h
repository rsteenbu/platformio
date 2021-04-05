#ifndef RELAY_H
#define RELAY_H
#define MYTZ TZ_America_Los_Angeles
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>
#include <Array.h>
#include <Wire.h>
#include "Adafruit_MCP23017.h"
#include <SPI.h>
#include <Adafruit_ADS1X15.h>

class Relay {
  int pin;
  bool backwards;
  int onVal, offVal;

  public:
    //constructors
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
    Relay (int a, Adafruit_MCP23017* b) {
    }

    bool on = false;
    bool scheduleOverride = false;
    time_t onTime = 0;
    time_t offTime = 0;
    char* name;

    void setName(char* a) {
      name = a;
    }

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

class McpRelay: public Relay {
  int pin;
  int onVal, offVal;
  Adafruit_MCP23017* mcp;

  public:
    McpRelay (int a, Adafruit_MCP23017* b) : Relay (a, b) {
      pin = a;
      mcp = b;
      onVal = HIGH;
      offVal = LOW;
    }

    void setup() {
      (*mcp).pinMode(pin, OUTPUT);
      (*mcp).digitalWrite(pin,offVal);
      configTime(MYTZ, "pool.ntp.org");
      offTime = time(nullptr);
    }

    void switchOn() {
      if (!on) {
        (*mcp).digitalWrite(pin, onVal);
        on = true;
	onTime = time(nullptr);
      }
    }

    void switchOff() {
      if (on) {
        (*mcp).digitalWrite(pin, offVal);
        on = false;
	offTime = time(nullptr);
      }
    }
};

class TimerRelay: public Relay {
  public:
    int runTime = 0;
    int startHour;
    int startMinute;
    float soilMoisturePercentage;
    float soilMoisturePercentageToRun = -1;
    Array<int,7> runDays;

    TimerRelay(int a): Relay(a) {}

    void setRuntime(int a) {
      runTime = a;
    }
    
    void setStartTime(int a, int b) {
      startHour = a;
      startMinute = b;
    }

    void setSoilMoisturePercentageToRun(int a) {
      soilMoisturePercentageToRun = a;
    }

    void setSoilMoisture(int a) {
      int const minSoilMoistureLevel = 300;
      int const maxSoilMoistureLevel = 700;

      float soilMoistureLevel = analogRead(a);

      if (soilMoistureLevel < minSoilMoistureLevel) {
	soilMoistureLevel = minSoilMoistureLevel;
      }
      if (soilMoistureLevel > maxSoilMoistureLevel) {
	soilMoistureLevel = maxSoilMoistureLevel;
      }
      soilMoisturePercentage = (1 - ((soilMoistureLevel - minSoilMoistureLevel) / (maxSoilMoistureLevel - minSoilMoistureLevel))) * 100;
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

   //  Array<int,7> irrigationDays;
   //  irrigationDays.fill(0);
   //  irrigationDays[3] = 1;  //Thursday
   //  irrigationDays[6] = 1;  //Sunday
   //  lightSwitch.setDaysFromArray(irrigationDays);
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

     if ( scheduleOverride ) {
       return 5; 
     }

     // if we're not on, turn it on if it's the right day and time
     if (!on) {
       if ( runDays[weekDay] && currMinuteOfDay == startMinuteOfDay )
       {
	 if ( soilMoisturePercentage > soilMoisturePercentageToRun ) {
	   return 3;
	 }
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
       if ( soilMoisturePercentage > soilMoisturePercentageToRun ) {
	 switchOff();
	 return 4;
       }
     }
     return 0;
   }

};
/*
class ScheduleRelay: public Relay {
  private:
    int const timesToSample = 10;
    int timesLight = 0;
    int timesDark = 0;

  public:
    int lightLevel;
    int dusk = 100;
    int nightOffHour = 0;
    int morningOnHour = 5;

    ScheduleRelay(int a): Relay(a) {}

    void setDusk(int a) {
      dusk = a;
    }
    void setLightLevel(int a) {
      lightLevel = a;
    }
  
    void setNightOffHour(int a) {
      nightOffHour = a;
    }
    void setMorningOnHour(int a) {
      morningOnHour = a;
    }

   int handle() {
     time_t now = time(nullptr);
     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;

     if ( scheduleOverride ) {
       return 5; 
     }
  
     // Switch on criteria: it's dark, the lights are not on and it's not the middle of the night
     if ( lightLevel < dusk && !on && !( currHour >= nightOffHour && currHour <= morningOnHour ) ) {
       timesLight = 0;
       timesDark++;

       if (timesDark > timesToSample) {
	 switchOn();
	 return 1;
       }
     }

     // Switch off criteria: the lights are on and either it's light or it's in the middle of the night
     if (on && (lightLevel > dusk || ( currHour >= nightOffHour && currHour <= morningOnHour ))) {
       timesDark = 0;
       timesLight++;

       if (timesLight > timesToSample) {
	 switchOff();
	 return 0;
       }
     }
   }
}
*/

#endif

