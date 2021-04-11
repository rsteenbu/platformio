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
#include <my_veml.h>

class IrrigationZones {

};

class Relay {
  int pin;
  bool backwards;
  int onVal, offVal;
  Adafruit_MCP23017* mcp;
  bool i2cRelay = false;

  public:
    //variables
    bool on = false;
    bool scheduleOverride = false;
    time_t onTime = 0;
    time_t offTime = 0;
    char* name;

    //constructors
    Relay (int a): pin(a) {
        onVal = HIGH;
        offVal = LOW;
    }
    Relay (int a, Adafruit_MCP23017* b) {
      pin = a;
      mcp = b;
      i2cRelay = true;
      onVal = HIGH;
      offVal = LOW;
    }
    Relay (int a, Adafruit_MCP23017* b, bool c) {
      pin = a;
      mcp = b;
      backwards = c;
      i2cRelay = true;
      if (backwards) {
        onVal = LOW;
        offVal = HIGH;
      } else {
        onVal = HIGH;
        offVal = LOW;
      }
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

    void setBackwards(bool a) {
      backwards = a;
    }

    void setScheduleOverride(bool a) {
      scheduleOverride = a;
    }

    bool getScheduleOverride() {
      return scheduleOverride;
    }

    void setup(const char* a) {
      name = new char[strlen(a)+1];
      strcpy(name,a);

      if (i2cRelay) {
	(*mcp).pinMode(pin, OUTPUT);
	(*mcp).digitalWrite(pin,offVal);
      } else {
	pinMode(pin, OUTPUT);
	digitalWrite(pin, offVal); // start off
      }
      configTime(MYTZ, "pool.ntp.org");
      offTime = time(nullptr);
    }


    void switchOn() {
      if (!on) {
        i2cRelay ? (*mcp).digitalWrite(pin, onVal) : digitalWrite(pin, onVal);
        on = true;
	onTime = time(nullptr);
      }
    }

    void switchOff() {
      if (on) {
        i2cRelay ? (*mcp).digitalWrite(pin, offVal) : digitalWrite(pin, offVal);
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

class GarageDoorRelay: public Relay {
  int REED_OPEN_PIN;
  int REED_CLOSED_PIN;
  int LED_OPEN_PIN = -1;
  int LED_CLOSED_PIN = -1;
  const int DOOR_OPEN = 0;
  const int DOOR_OPENING = 1;
  const int DOOR_CLOSED = 2;
  const int DOOR_CLOSING = 3;

  public:
    //constructurs
    GarageDoorRelay(int a, int b, int c ): Relay(a) {
      REED_OPEN_PIN = b;
      REED_CLOSED_PIN = c;

      // Since the other end of the reed switch is connected to ground, we need
      // to pull-up the reed switch pin internally.
      pinMode(REED_OPEN_PIN, INPUT_PULLUP);
      pinMode(REED_CLOSED_PIN, INPUT_PULLUP);
    }
    GarageDoorRelay(int a, int b, int c, int d, int e ): Relay(a) {
      REED_OPEN_PIN = b;
      REED_CLOSED_PIN = c;
      LED_OPEN_PIN = d;
      LED_CLOSED_PIN = e;

      // Since the other end of the reed switch is connected to ground, we need
      // to pull-up the reed switch pin internally.
      pinMode(REED_OPEN_PIN, INPUT_PULLUP);
      pinMode(LED_OPEN_PIN, OUTPUT);
      pinMode(REED_CLOSED_PIN, INPUT_PULLUP);
      pinMode(LED_CLOSED_PIN, OUTPUT);

      // prepare LED Pins
      pinMode(LED_BUILTIN, OUTPUT);
      digitalWrite(LED_BUILTIN, LOW);
    }

    int status;

    void operate() {
	onTime = time(nullptr);
        switchOn();
	delay(100);
        switchOff();
    }

    const char* statusWord() {
      switch (status) {
	case 0:
	  return "OPEN";
	  break;
	case 1:
	  return "OPENING";
	  break;
	case 2:
	  return "CLOSED";
	  break;
	case 3:
	  return "CLOSING";
	  break;
      }
      return "UKNOWN";
    }

   int handle() {
     int doorOpen = digitalRead(REED_OPEN_PIN); // Check to see of the door is open
     if (doorOpen == LOW) { // Door detected is in the open position
       if (status != DOOR_OPEN) {
	 if (LED_OPEN_PIN) digitalWrite(LED_OPEN_PIN, HIGH); // Turn the LED on
	 status = DOOR_OPEN;
	 return 1;
       }
     } else { // Door is not in the open position
       if (status == DOOR_OPEN ) {
	 if (LED_OPEN_PIN) digitalWrite(LED_OPEN_PIN, LOW); // Turn the LED off
	 status = DOOR_CLOSING;
	 return 1;
       }
     }

     int doorClosed = digitalRead(REED_CLOSED_PIN); // Check to see of the door is closed
     if (doorClosed == LOW) // Door detected in the closed position
     {
       if (status != DOOR_CLOSED) {
	 if (LED_CLOSED_PIN) digitalWrite(LED_CLOSED_PIN, HIGH); // Turn the LED on
	 status = DOOR_CLOSED;
	 return 1;
       }
     } else { // Door is not in the closed position
       if (status == DOOR_CLOSED) {
	 if (LED_CLOSED_PIN) digitalWrite(LED_CLOSED_PIN, LOW); // Turn the LED off
	 status = DOOR_OPENING;
	 return 1;
       }
     }
     return 0;
   }
};

class TimerRelay: public Relay {
  public:
    int runTime = 0;
    int startHour;
    int startMinute;
    Array<int,7> runDays;

    //constructurs
    TimerRelay(int a): Relay(a) {
      setEveryDayOn();
    }
    TimerRelay (int a, Adafruit_MCP23017* b): Relay(a, b) {
      setEveryDayOn();
    }

    TimerRelay (int a, Adafruit_MCP23017* b, bool c): Relay(a, b, c) {
      setEveryDayOn();
    }

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

   bool isTimeToStart() {
     time_t now = time(nullptr);
     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;
     int currMinute = timeinfo->tm_min;
     int weekDay = timeinfo->tm_wday;

     int currMinuteOfDay = (currHour * 60) + currMinute;
     int startMinuteOfDay = (startHour * 60) + startMinute;

     return runDays[weekDay] && currMinuteOfDay == startMinuteOfDay;
   }

   bool isTimeToStop() {
     time_t now = time(nullptr);

     return now >= onTime + (runTime * 60);
   }

   int handle() {
     // if we don't have runtime set, then just return
     if ( runTime == 0 ) return 0;
     if ( scheduleOverride ) return 5;

     // if we're not on, turn it on if it's the right day and time
     if ( !on && isTimeToStart() ) {
       return 1;
     } 

     // if we're on, turn it off if it's been more than than the time to run 
     if ( on && isTimeToStop() ) {
	 switchOff();
	 return 2;
       }

     return 0;
   }
};

class IrrigationRelay: public TimerRelay {
  int soilPin;
  bool i2cSoilMoistureSensor = false;
  int soilMoisturePercentageToRun = -1;
  // default limits - analog pin read frontyard
  double drySoilMoistureLevel = 660;
  double wetSoilMoistureLevel = 330;
  Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */

  public:
    double soilMoisturePercentage;
    double soilMoistureLevel = -1;

    //constructors
    IrrigationRelay (int a): TimerRelay(a) { }
    IrrigationRelay (int a, Adafruit_MCP23017* b): TimerRelay(a, b) { }
    IrrigationRelay (int a, Adafruit_MCP23017* b, bool c): TimerRelay(a, b, c) { }

    // turn on the soilMoisture check at soilMoisturePercentageToRun
    void setSoilMoistureSensor(int a, int b) {
      soilPin = a;
      soilMoisturePercentageToRun = b;
    }

    void setSoilMoistureSensor(uint8_t a, int b, int c) {
      i2cSoilMoistureSensor = true;
      ads.begin(a);
      soilPin = b;
      soilMoisturePercentageToRun = c;
    }

    void setSoilMoistureLimits(int a, int b) {
      drySoilMoistureLevel = a;
      wetSoilMoistureLevel = b;
    }

    bool checkSoilMoisture() {
      double calibratedSoilMoistureLevel = soilMoistureLevel;

      if (i2cSoilMoistureSensor) {
	soilMoistureLevel = ads.readADC_SingleEnded(soilPin);
      } else {
	soilMoistureLevel = analogRead(soilPin);
      }

      if (calibratedSoilMoistureLevel > drySoilMoistureLevel) {
	calibratedSoilMoistureLevel = drySoilMoistureLevel;
      }
      if (calibratedSoilMoistureLevel < wetSoilMoistureLevel) {
	calibratedSoilMoistureLevel = wetSoilMoistureLevel;
      }
      
      soilMoisturePercentage = 100 - (((calibratedSoilMoistureLevel - wetSoilMoistureLevel) / (drySoilMoistureLevel - wetSoilMoistureLevel)) * 100);
      
      return soilMoisturePercentage < soilMoisturePercentageToRun;
    }

   int handle() {
     // set the soilMoisture level on every loop
     bool soilDry = checkSoilMoisture();

     // if we don't have runtime set, then just return
     if ( runTime == 0 ) return 0;
     if ( scheduleOverride ) return 5;

     // if we're not on, turn it on if it's the right day and time
     if ( !on && isTimeToStart() ) {
       if ( !soilDry ) return 3;
       switchOn();
       return 1;
     } 

     // if we're on, turn it off if it's been more than than the time to run or if it's started raining
     if ( on && isTimeToStop() ) {
	 switchOff();
	 return 2;
       }
     if ( on && soilMoisturePercentage > soilMoisturePercentageToRun ) {
	 switchOff();
	 return 4;
     }

     return -1;
   }
};

class ScheduleRelay: public Relay {
  const int MAX_SLOTS = 4;

  const int hourIndex = 0;
  const int minuteIndex = 1;
  int onTimes[4][2];
  int offTimes[4][2];
  int slotsTaken = -1;

  bool findTime(int hour, int min, int timeArray[4][2] ) {
    for (int i = 0; i <= slotsTaken; i++) {
      if (timeArray[i][hourIndex] == hour && timeArray[i][minuteIndex] == min) {
	return true;
      }
    }
    return false;
  }

  public:
    ScheduleRelay (int a): Relay(a) {}

    //on hour, on minute, off hour, off minute
    bool setOnOffTimes(int a, int b, int c, int d) {
      if (slotsTaken == MAX_SLOTS) {
	return false;
      }
      slotsTaken++;
    
      onTimes[slotsTaken][hourIndex] = a;
      onTimes[slotsTaken][minuteIndex] = b;
      offTimes[slotsTaken][hourIndex] = c;
      offTimes[slotsTaken][minuteIndex] = d;

      return true;
    }

   int handle() {
     time_t now = time(nullptr);
     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;
     int currMinute = timeinfo->tm_min;

     if ( scheduleOverride ) {
       return 5; 
     }

     if ( ! on && findTime(currHour, currMinute, onTimes) ) {
       switchOn();
       return 1;
     }

     if ( on && findTime(currHour, currMinute, offTimes) ) {
       switchOff();
       return 2;
     }

     return 0;
   }
};

class DuskToDawnScheduleRelay: public ScheduleRelay {
  int const timesToSample = 10;
  int timesLight = 0;
  int timesDark = 0;
  int dusk = 100;
  int nightOffHour = 0;
  int morningOnHour = 5;
  Veml veml;
  bool vemlSensor = false;

  public:
    int lightLevel;

    DuskToDawnScheduleRelay (int a): ScheduleRelay(a) {}

    bool setVemlLightSensor() {
      vemlSensor = true;

      if (veml.setup()) {
	return true;
      } else { 
	return false;
      }
    }

    void setMorningOnHour(int a) {
      morningOnHour = a;
    }

    void setNightOffHour(int a) {
      nightOffHour = a;
    }

    void setDusk(int a) {
      dusk = a;
    }

   int handle() {
     time_t now = time(nullptr);
     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;

     lightLevel = veml.readLux();

     if ( scheduleOverride ) {
       return 5; 
     }

     // Switch on criteria: it's dark, the lights are not on and it's not the middle of the night
     if ( lightLevel < dusk && ! on && !( currHour >= nightOffHour && currHour <= morningOnHour ) ) {
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
	 return 2;
       }
     }

     return 0;
   }
};

#endif

