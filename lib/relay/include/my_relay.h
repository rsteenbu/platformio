#ifndef RELAY_H
#define RELAY_H

#ifdef ESP32
#include <ESPmDNS.h>
#else
#include <sys/time.h>                   // struct timeval
#include <TZ.h>
#define MYTZ TZ_America_Los_Angeles
#endif

#include <time.h>                       // time() ctime()
#include <Array.h>
#include <Wire.h>
#include "Adafruit_MCP23X17.h"
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include <my_veml.h>


class Relay {
  int onVal = HIGH;
  int offVal = LOW;
  Adafruit_MCP23X17* mcp;
  bool i2cPins = false;
  const char* ntpServer = "pool.ntp.org";
  const long  gmtOffset_sec = 8*60*60*-1;
  const int   daylightOffset_sec = 3600;

  protected:
    int pin;
    void internalOn();
    void internalOff();

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
    Relay (int a);
    Relay (int a, Adafruit_MCP23X17* b);

    void setBackwards();
    void setScheduleOverride(bool a);
    bool getScheduleOverride();
    int status();
    void setup(const char* a);
    void setup();
    void switchOn();
    void switchOff();
    const char* state();
};

class GarageDoorRelay: public Relay {
  bool i2cPins = false;
  bool useLeds = false;
  Adafruit_MCP23X17* mcp;
  int DOOR_PIN;
  int REED_OPEN_PIN;
  int REED_CLOSED_PIN;
  int LED_OPEN_PIN;
  int LED_CLOSED_PIN;
  const int DOOR_OPEN = 0;
  const int DOOR_OPENING = 1;
  const int DOOR_CLOSED = 2;
  const int DOOR_CLOSING = 3;

  public:
    //variables
    int doorState;

    //constructurs
    GarageDoorRelay(int a, int b, int c );
    GarageDoorRelay(int a, int b, int c, int d, int e );
    GarageDoorRelay(int a, int b, int c, int d, int e, Adafruit_MCP23X17* f);
   
    int status();
    void setup(const char* a);
    void operate();
    const char* state();
    bool handle();
};

class TimerRelay: public Relay {

  protected:
    time_t now, prevTime = 0;
    int minutes = 0, seconds = 0;
    int secondsLeft = 0;
    int initialRunTime = 0;

    void setTimeLeftToRun();
    void setNextTimeToRun();

  public:
    int runTime = 0;
    bool active = true;
    char timeLeftToRun[6];
    char nextTimeToRun[18];
    Array<int,7> runDays;
    Array<int,5> startTimesOfDay;

    //constructurs
    TimerRelay(int a);
    TimerRelay (int a, Adafruit_MCP23X17* b);

    void setActive();
    void setInActive();
    void setRuntime(int a);
    void setRuntimeMinutes(int a);
    int getSecondsLeft();
    void addTimeToRun(int a);
    void switchOn();
    void switchOff();
    bool setStartTimeFromString(const char *a);
    bool setStartTime(int a, int b);
    void setEveryOtherDayOn();
    void setEveryDayOn();
    void setEveryDayOff();
    void setDaysFromArray(Array<int,7> & array);
    void setSpecificDayOn(int n);
    void getWeekSchedule(char weekSchedule[8]);
    int checkDayToRun(int weekDay);
    int checkDayToRun();
    void checkStartTime(String &timesToStart);
    bool isTimeToStart();
    bool isTimeToStop();
    bool handle();
};

// TODO: method to display time left to run
// TODO: status of next scheduled run time in days, hours, minutes
class IrrigationRelay: public TimerRelay {
  int moisturePin;
  bool i2cMoistureSensor = false;
  int moisturePercentageToRun = -1;
  // default limits - analog pin read frontyard
  int dryMoistureLevel = 660;
  int wetMoistureLevel = 330;
  Adafruit_ADS1015 ads;     /* Use this for the 12-bit version */

  public:
    bool dry = true;
    bool moistureSensor = false;
    int moisturePercentage = -1;
    int moistureLevel = -1;

    //constructors
    IrrigationRelay (int a);
    IrrigationRelay (int a, Adafruit_MCP23X17* b);
    //"patio_pots",  7,       true,      "7:00",              3,            , '1111111'
    IrrigationRelay (const char* a, int b, bool c, const char* d, int e, bool f, Adafruit_MCP23X17* g);

    // turn on the moisture check at moisturePercentageToRun
    void setMoistureSensor(int a, int b);
    void setMoistureSensor(uint8_t a, int b, int c);
    void setMoistureLimits(int a, int b);
    void checkMoisture();
    void setMoistureLevel(int n);
    const char* state();
    bool handle();
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

    bool findTime(int hour, int min, int timeArray[4][2] );

  public:
    ScheduleRelay (int a);

    //on hour, on minute, off hour, off minute
    bool setOnOffTimes(int a, int b, int c, int d);
    bool handle();
};

class DuskToDawnScheduleRelay: public ScheduleRelay {
  int const timesToSample = 10;
  int timesLight = 0;
  int timesDark = 0;
  int dusk = 70;
  int nightOffHour = 0;
  int morningOnHour = 5;
  Veml veml;
  bool vemlSensor = false;

  public:
    int lightLevel = -1;
    DuskToDawnScheduleRelay (int a);
    bool setVemlLightSensor();
    void setNightOffOnHours(int a, int b);
    void setDusk(int a);
    bool handle();
};

#endif
