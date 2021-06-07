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

class Relay {
  int pin;
  int onVal = HIGH;
  int offVal = LOW;
  Adafruit_MCP23017* mcp;
  bool i2cPins = false;

  protected:
    void internalOn() {
      if (!on) {
        i2cPins ? (*mcp).digitalWrite(pin, onVal) : digitalWrite(pin, onVal);
        on = true;
	onTime = time(nullptr);
	struct tm *timeinfo = localtime(&onTime);
	strftime (prettyOnTime,18,"%D %T",timeinfo);
      }
    }

    void internalOff() {
      if (on) {
        i2cPins ? (*mcp).digitalWrite(pin, offVal) : digitalWrite(pin, offVal);
        on = false;
	offTime = time(nullptr);
	struct tm *timeinfo = localtime(&offTime);
	strftime (prettyOffTime,18,"%D %T",timeinfo);
      }
    }

  public:
    //variables
    bool on = false;
    bool scheduleOverride = false;
    time_t onTime = 0;
    time_t offTime = 0;
    char prettyOffTime[18];
    char prettyOnTime[18];
    char* name;

    //constructors
    Relay (int a): pin(a) {}
    Relay (int a, Adafruit_MCP23017* b) {
      pin = a;
      mcp = b;
      i2cPins = true;
    }

    void setBackwards() {
      onVal = LOW;
      offVal = HIGH;
    }

    void setScheduleOverride(bool a) {
      scheduleOverride = a;
    }

    bool getScheduleOverride() {
      return scheduleOverride;
    }

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

      if (i2cPins) {
	(*mcp).pinMode(pin, OUTPUT);
	(*mcp).digitalWrite(pin,offVal);
      } else {
	pinMode(pin, OUTPUT);
	digitalWrite(pin, offVal); // start off
      }
      configTime(MYTZ, "pool.ntp.org");
      onTime = offTime = time(nullptr);
      strcpy(prettyOnTime,"None");
      strcpy(prettyOffTime,"None");
    }

    void switchOn() {
      scheduleOverride = true;
      internalOn();
    }
    
    void switchOff() {
      scheduleOverride = false;
      internalOff();
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
    //variables
    int doorState;

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

    int status() {
      return doorState;
    }

    void operate() {
	onTime = time(nullptr);
        internalOn();
	delay(100);
        internalOff();
    }

    const char* state() {
      switch (doorState) {
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

   bool handle() {
     int doorOpen = digitalRead(REED_OPEN_PIN); // Check to see of the door is open
     if (doorOpen == LOW) { // Door detected is in the open position
       if (doorState != DOOR_OPEN) {
	 if (LED_OPEN_PIN) digitalWrite(LED_OPEN_PIN, HIGH); // Turn the LED on
	 doorState = DOOR_OPEN;
	 return true;
       }
     } else { // Door is not in the open position
       if (doorState == DOOR_OPEN ) {
	 if (LED_OPEN_PIN) digitalWrite(LED_OPEN_PIN, LOW); // Turn the LED off
	 doorState = DOOR_CLOSING;
	 return true;
       }
     }

     int doorClosed = digitalRead(REED_CLOSED_PIN); // Check to see of the door is closed
     if (doorClosed == LOW) // Door detected in the closed position
     {
       if (doorState != DOOR_CLOSED) {
	 if (LED_CLOSED_PIN) digitalWrite(LED_CLOSED_PIN, HIGH); // Turn the LED on
	 doorState = DOOR_CLOSED;
	 return true;
       }
     } else { // Door is not in the closed position
       if (doorState == DOOR_CLOSED) {
	 if (LED_CLOSED_PIN) digitalWrite(LED_CLOSED_PIN, LOW); // Turn the LED off
	 doorState = DOOR_OPENING;
	 return true;
       }
     }
     return false;
   }
};

class TimerRelay: public Relay {

  protected:
    time_t now, prevTime = 0;

    void setTimeLeftToRun() {
      if ( ! on ) { 
        strcpy(timeLeftToRun, "Not running");
      } else {
        int secondsLeft = (runTime * 60) - (now - onTime);
        int minutes = secondsLeft / 60;
        int seconds = secondsLeft % 60;
  
        sprintf(timeLeftToRun, "%d:%02d", minutes, seconds);
      }
    }

    void setNextTimeToRun() {
      struct tm *timeinfo = localtime(&now);
      int currWeekDay = timeinfo->tm_wday;
      int currHour = timeinfo->tm_hour;
      int currMinute = timeinfo->tm_min;
      int currSecond = timeinfo->tm_sec;

      if ( startTimesOfDay.size() == 0 ) {
        return;
      }

      int currMinuteOfDay = (currHour * 60) + currMinute;

      int timeFound = 0;
      int n;
      for ( n=0 ; n<7 ; n++ ) {
        int dayOfWeek = (n + currWeekDay) & 7;

	// Skip checking this day if we're not scheduled to run
        if (! runDays[dayOfWeek]) continue;

        //check each startTime
	for (int i = 0; i < int(startTimesOfDay.size()); i++) {
	  // if it's today but the time we're looking it was before now, skip it
	  if ( (dayOfWeek == currWeekDay) && (startTimesOfDay[i] < currMinuteOfDay) ) continue;
	  // if we found a start time already and this is bigger, skip it
	  if ( timeFound && startTimesOfDay[i] > timeFound ) continue;

	  timeFound = startTimesOfDay[i];
	}
	
	if ( timeFound ) {
	  time_t startTime = now - currMinuteOfDay*60 + n*24*60*60 + timeFound*60 - currSecond;
	  struct tm *timeinfo = localtime(&startTime);
	  strftime (nextTimeToRun,18,"%D %T",timeinfo);
	  return;
	} 
      }
    }

  public:
    int runTime = 0;
    char timeLeftToRun[12];
    char nextTimeToRun[18];
    Array<int,7> runDays;
    Array<int,5> startTimesOfDay;

    //constructurs
    TimerRelay(int a): Relay(a) {
      setEveryDayOn();
    }
    TimerRelay (int a, Adafruit_MCP23017* b): Relay(a, b) {
      setEveryDayOn();
    }

    void setRuntime(int a) {
      runTime = a;
    }

    void setStartTimes(char *a) {
      bool processMinutes = false;
      char hours[2];
      char minutes[2];
      int n = 0;
      for (int i = 0; a[i] != '\0'; i++){
	if (! processMinutes) {
	  hours[n++] = a[i];

	  if (a[i] == ':') {
	    processMinutes = true;
	    n = 0;
	    continue;
	  }
	} else {
	  minutes[n++] = a[i];
	}
      }
    }

    void setStartTime(int a, int b) {
      int startMinuteOfDay = a * 60 + b;
      if ( ! startTimesOfDay.full() ) {
	startTimesOfDay.push_back(startMinuteOfDay);
      }
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

   bool isTimeToStart() {
     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;
     int currMinute = timeinfo->tm_min;
     int weekDay = timeinfo->tm_wday;

     for (int i = 0; i < int(startTimesOfDay.size()); i++) {
       if (runDays[weekDay] && (((currHour * 60) + currMinute) == startTimesOfDay[i]) ) {
	 return true;
       } 
     }
     return false;
   }

   bool isTimeToStop() {
     return now >= onTime + (runTime * 60);
   }

   bool handle() {
     prevTime = now;
     now = time(nullptr);

     //uptime the time left to run every second
     if ( now != prevTime ) { 
       setTimeLeftToRun();
       setNextTimeToRun();
     }

     if ( ( now == prevTime ) || ( now % 60 != 0 ) ) return false;
     
     // if we don't have runtime set, then just return
     if ( runTime == 0 ) return false;

      // if we're on, turn it off if it's been more than than the time to run or if it's started raining
      // if the scheduleOverride is on, turn it off if the timer expires and resume normal
      // operation
     if ( on && isTimeToStop() ) {
       switchOff();
       return true;
     }

     if ( scheduleOverride ) return false;

     // if we're not on, turn it on if it's the right day and time
     if ( !on && isTimeToStart() ) {
       internalOn();
       return true;
     } 

     return false;
   }
};

// TODO: method to display time left to run
// TODO: status of next scheduled run time in days, hours, minutes
class IrrigationRelay: public TimerRelay {
  int soilPin;
  bool i2cSoilMoistureSensor = false;
  int soilMoisturePercentageToRun = -1;
  // default limits - analog pin read frontyard
  double drySoilMoistureLevel = 660;
  double wetSoilMoistureLevel = 330;
  Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */

  public:
    bool soilMoistureSensor = false;
    double soilMoisturePercentage = -1;
    double soilMoistureLevel = -1;
    bool soilDry = true;

    //constructors
    IrrigationRelay (int a): TimerRelay(a) { }
    IrrigationRelay (int a, Adafruit_MCP23017* b): TimerRelay(a, b) { }

    // turn on the soilMoisture check at soilMoisturePercentageToRun
    void setSoilMoistureSensor(int a, int b) {
      soilMoistureSensor = true;
      soilPin = a;
      soilMoisturePercentageToRun = b;
    }

    void setSoilMoistureSensor(uint8_t a, int b, int c) {
      soilMoistureSensor = true;
      i2cSoilMoistureSensor = true;
      ads.begin(a);
      soilPin = b;
      soilMoisturePercentageToRun = c;
    }

    void setSoilMoistureLimits(int a, int b) {
      drySoilMoistureLevel = a;
      wetSoilMoistureLevel = b;
    }

    void checkSoilMoisture() {
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
      
      soilDry = (soilMoisturePercentage < soilMoisturePercentageToRun) ?  true : false;
    }

    const char* state() {
      if (! soilDry) return "wet";

      if (on) {
	return "on";
      } else { 
	return "off";
      }
    }

    bool handle() {
      prevTime = now;
      now = time(nullptr);

      //uptime the time left to run every second
      if ( now != prevTime ) { 
	setTimeLeftToRun();
	setNextTimeToRun();
      }

      // only process the rest every 5 seconds
      if ( ( now == prevTime ) || ( now % 5 != 0 ) ) return false;

      // set the soilMoisture level on every loop
      if (soilMoistureSensor) checkSoilMoisture();

      // if we don't have runtime set, then just return
      if ( runTime == 0 ) return false;

      // if we're on, turn it off if it's been more than than the time to run or if it's started raining
      // if the scheduleOverride is on, turn it off if the timer expires and resume normal
      // operation
      if ( on && isTimeToStop() ) {
	switchOff();
	return true;
      }

      if ( scheduleOverride ) return false;

      // if we're on it started raining, turn it off
      // but ignore the scheduleOvveride and stay running if it's set
      if ( on && !soilDry ) {
	internalOff();
	return true;
      }

      // if we're not on, turn it on if it's the right day and time
      if ( !on && isTimeToStart() ) {
	if ( ! soilDry ) {
	  return false;
	}
	internalOn();
	return true;
      } 

      return false;
    }
};

class ScheduleRelay: public Relay {
  const int MAX_SLOTS = 4;
  const int hourIndex = 0;
  const int minuteIndex = 1;
  int onTimes[4][2];
  int offTimes[4][2];
  int slotsTaken = -1;

  protected:
    time_t now, prevTime = 0;

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

   bool handle() {
     prevTime = now;
     now = time(nullptr);
     if ( ( now == prevTime ) || ( now % 5 != 0 ) ) return false;

     if ( scheduleOverride )  return false;

     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;
     int currMinute = timeinfo->tm_min;
     if ( ! on && findTime(currHour, currMinute, onTimes) ) {
       internalOn();
       return true;
     }

     if ( on && findTime(currHour, currMinute, offTimes) ) {
       internalOff();
       return true;
     }

     return false;
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
    int lightLevel = -1;

    DuskToDawnScheduleRelay (int a): ScheduleRelay(a) {}

    bool setVemlLightSensor() {

      if (veml.setup()) {
        vemlSensor = true;
      } else { 
        vemlSensor = false;
      }
      return vemlSensor;
    }

    void setNightOffOnHours(int a, int b) {
      nightOffHour = a;
      morningOnHour = b;
    }

    void setDusk(int a) {
      dusk = a;
    }

   bool handle() {
     prevTime = now;
     now = time(nullptr);
     if ( ( now == prevTime ) || ( now % 5 != 0 ) ) return false;

     if (vemlSensor) {
       lightLevel = veml.readLux();
     }

     if ( scheduleOverride ) {
       return false; 
     }

     struct tm *timeinfo = localtime(&now);
     int currHour = timeinfo->tm_hour;
     // Switch on criteria: it's dark, the lights are not on and it's not the middle of the night
     if ( lightLevel < dusk && ! on && !( currHour >= nightOffHour && currHour <= morningOnHour ) ) {
       timesLight = 0;
       timesDark++;

       if (timesDark > timesToSample) {
	 internalOn();
	 return true;
       }
     }

     // Switch off criteria: the lights are on and either it's light or it's in the middle of the night
     if (on && (lightLevel > dusk || ( currHour >= nightOffHour && currHour <= morningOnHour ))) {
       timesDark = 0;
       timesLight++;

       if (timesLight > timesToSample) {
	 internalOff();
	 return true;
       }
     }

     return false;
   }
};

#endif

