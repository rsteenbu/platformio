#include "my_relay.h"

void Relay::internalOn() {
  if (!on) {
    i2cPins ? (*mcp).digitalWrite(pin, onVal) : digitalWrite(pin, onVal);
    on = true;
    onTime = time(nullptr);
    struct tm *timeinfo = localtime(&onTime);
    strftime (prettyOnTime,18,"%D %T",timeinfo);
  }
}

void Relay::internalOff() {
  if (on) {
    i2cPins ? (*mcp).digitalWrite(pin, offVal) : digitalWrite(pin, offVal);
    on = false;
    offTime = time(nullptr);
    struct tm *timeinfo = localtime(&offTime);
    strftime (prettyOffTime,18,"%D %T",timeinfo);
  }
}

//constructors
Relay::Relay (int a): pin(a) {}
Relay::Relay (int a, Adafruit_MCP23X17* b) {
  pin = a;
  mcp = b;
  i2cPins = true;
}

void Relay::setBackwards() {
  onVal = LOW;
  offVal = HIGH;
}

void Relay::setScheduleOverride(bool a) {
  scheduleOverride = a;
}

bool Relay::getScheduleOverride() {
  return scheduleOverride;
}

int Relay::status() {
  if (on) {
    return 1;
  } else {
    return 0;
  }
}

void Relay::setup(const char* a) {
  name = new char[strlen(a)+1];
  strcpy(name,a);
  this->setup();
}

void Relay::setup() {
  if (i2cPins) {
    (*mcp).pinMode(pin, OUTPUT);
    (*mcp).digitalWrite(pin, offVal); // start off
  } else {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, offVal); // start off
  }

  // iternate through one on off cycle to work the kinks out
  internalOn(); internalOff();

#ifdef ESP32
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
#else
  configTime(MYTZ, ntpServer);
#endif
  onTime = offTime = time(nullptr);
  strcpy(prettyOnTime,"None");
  strcpy(prettyOffTime,"None");
}

void Relay::switchOn() {
  internalOn();
}

void Relay::switchOff() {
  internalOff();
}

const char* Relay::state() {
  if (on) {
    return "on";
  } else { 
    return "off";
  }
}


    //constructurs
GarageDoorRelay::GarageDoorRelay(int a, int b, int c ): Relay(a) {
  DOOR_PIN = a;
  REED_OPEN_PIN = b;
  REED_CLOSED_PIN = c;

  useLeds = false;
  // Since the other end of the reed switch is connected to ground, we need
  // to pull-up the reed switch pin internally.
  pinMode(REED_OPEN_PIN, INPUT_PULLUP);
  pinMode(REED_CLOSED_PIN, INPUT_PULLUP);
}
GarageDoorRelay::GarageDoorRelay(int a, int b, int c, int d, int e ): Relay(a) {
  DOOR_PIN = a;
  REED_OPEN_PIN = b;
  REED_CLOSED_PIN = c;
  LED_OPEN_PIN = d;
  LED_CLOSED_PIN = e;

  useLeds = true;
}
GarageDoorRelay::GarageDoorRelay(int a, int b, int c, int d, int e, Adafruit_MCP23X17* f): Relay(a, f) {
  DOOR_PIN = a;
  REED_OPEN_PIN = b;
  REED_CLOSED_PIN = c;
  LED_OPEN_PIN = d;
  LED_CLOSED_PIN = e;
  mcp = f;

  i2cPins = true;
  useLeds = true;
}

int GarageDoorRelay::status() {
  return doorState;
}

void GarageDoorRelay::setup(const char* a) {
  Relay::pin = DOOR_PIN;
  Relay::setup(a);

  // Since the other end of the reed switch is connected to ground, we need
  // to pull-up the reed switch pin internally.
  i2cPins ? (*mcp).pinMode(REED_OPEN_PIN, INPUT_PULLUP) : 
    pinMode(REED_OPEN_PIN, INPUT_PULLUP);
  i2cPins ? (*mcp).pinMode(REED_CLOSED_PIN, INPUT_PULLUP) : 
    pinMode(REED_CLOSED_PIN, INPUT_PULLUP);

  if (useLeds) {
    i2cPins ? (*mcp).pinMode(LED_OPEN_PIN, OUTPUT) : 
      pinMode(LED_OPEN_PIN, OUTPUT);
    i2cPins ? (*mcp).pinMode(LED_CLOSED_PIN, OUTPUT) : 
      pinMode(LED_CLOSED_PIN, OUTPUT);
    // Set the LED's to off initially
    i2cPins ? (*mcp).digitalWrite(LED_OPEN_PIN, LOW) : 
      digitalWrite(LED_OPEN_PIN, LOW); 
    i2cPins ? (*mcp).digitalWrite(LED_CLOSED_PIN, LOW) : 
      digitalWrite(LED_CLOSED_PIN, LOW);
  }
}

void GarageDoorRelay::operate() {
  onTime = time(nullptr);
  switchOn();
  delay(100);
  switchOff();
}

const char* GarageDoorRelay::state() {
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

bool GarageDoorRelay::handle() {
  int doorOpen = i2cPins ? (*mcp).digitalRead(REED_OPEN_PIN) : digitalRead(REED_OPEN_PIN); // Check to see of the door is open
  if (doorOpen == LOW) { // Door detected is in the open position
    if (doorState != DOOR_OPEN) {
      if (useLeds) i2cPins ? (*mcp).digitalWrite(LED_OPEN_PIN, HIGH) : digitalWrite(LED_OPEN_PIN, HIGH); // Turn the LED on
      doorState = DOOR_OPEN;
      return true;
    }
  } else { // Door is not in the open position
    if (doorState == DOOR_OPEN ) {
      if (useLeds) i2cPins ? (*mcp).digitalWrite(LED_OPEN_PIN, LOW) : digitalWrite(LED_OPEN_PIN, LOW); // Turn the LED off
      doorState = DOOR_CLOSING;
      return true;
    }
  }

  int doorClosed = i2cPins ? (*mcp).digitalRead(REED_CLOSED_PIN) : digitalRead(REED_CLOSED_PIN); // Check to see of the door is closed
  if (doorClosed == LOW) // Door detected in the closed position
  {
    if (doorState != DOOR_CLOSED) {
      if (LED_CLOSED_PIN) i2cPins ? (*mcp).digitalWrite(LED_CLOSED_PIN, HIGH) : digitalWrite(LED_CLOSED_PIN, HIGH); // Turn the LED on
      doorState = DOOR_CLOSED;
      return true;
    }
  } else { // Door is not in the closed position
    if (doorState == DOOR_CLOSED) {
      if (LED_CLOSED_PIN) i2cPins ? (*mcp).digitalWrite(LED_CLOSED_PIN, LOW) : digitalWrite(LED_CLOSED_PIN, LOW); // Turn the LED off
      doorState = DOOR_OPENING;
      return true;
    }
  }
  return false;
}

// TimerRelay constructurs
TimerRelay::TimerRelay(int a): Relay(a) {
  setEveryDayOn();
  //preferences.begin("TimerRelay", false);
}
TimerRelay::TimerRelay (int a, Adafruit_MCP23X17* b): Relay(a, b) {
  setEveryDayOn();
  //preferences.begin("TimerRelay", false);
}

void TimerRelay::setTimeLeftToRun() {
  if ( on ) { 
    secondsLeft = runTime - (now - onTime);
    minutes = secondsLeft / 60;
    seconds = secondsLeft % 60;
  } else {
    minutes = 0;
    seconds = 0;
  }
  sprintf(timeLeftToRun, "%2d:%02d", minutes, seconds);
}

void TimerRelay::setNextTimeToRun() {
  struct tm *timeinfo = localtime(&now);
  int thisDay = timeinfo->tm_wday;
  int thisHour = timeinfo->tm_hour;
  int thisMinute = timeinfo->tm_min;
  int thisSecond = timeinfo->tm_sec;

  // if there are no start times set, just return
  if ( startTimesOfDay.size() == 0 ) {
    return;
  }

  int currMinuteOfDay = (thisHour * 60) + thisMinute;

  int startMinute = 0;
  int n;
  for ( n=0 ; n<7 ; n++ ) {
    int dayFromThisDay = (n + thisDay) & 7;

    // Skip checking this day if we're not scheduled to run
    if (! runDays[dayFromThisDay]) continue;

    //check each startTime(inMinutes)
    for (int i = 0; i < int(startTimesOfDay.size()); i++) {
      // if it's today but the time we're looking at was before now, skip it
      if ( (dayFromThisDay == thisDay) && (startTimesOfDay[i] < currMinuteOfDay) ) continue;
      // if we found a start time already and this one is bigger, skip it and keep the one we have
      if ( startMinute && startTimesOfDay[i] > startMinute ) continue;

      startMinute = startTimesOfDay[i];
    }

    if ( startMinute ) {
      //                 (midnight of today) + (days from this day) + (the start time) 
      time_t startTime = (now - currMinuteOfDay*60 - thisSecond) + (n*24*60*60) + startMinute*60;
      struct tm *timeinfo = localtime(&startTime);
      strftime (nextTimeToRun,18,"%D %T",timeinfo);
      return;
    } 
  }
}

void TimerRelay::setActive() {
  active = true;
  //preferences.putBool("active", active);
}

void TimerRelay::setInActive() {
  active = false;
  //preferences.putBool("active", active);
}

void TimerRelay::setRuntime(int a) {
  initialRunTime = a;
}

void TimerRelay::setRuntimeMinutes(int a) {
  initialRunTime = a*60;
}

int TimerRelay::getSecondsLeft() {
  return secondsLeft;
}

void TimerRelay::addTimeToRun(int a) {
  runTime += a;
  if (!on) {
    Relay::switchOn();
  }
  setTimeLeftToRun();
}

void TimerRelay::switchOn() {
  runTime = initialRunTime;
  Relay::switchOn();
}

void TimerRelay::switchOff() {
  runTime = 0;
  Relay::switchOff();
} 

bool TimerRelay::setStartTimeFromString(const char *a) {
  if (  startTimesOfDay.full() ) { return false; }

  bool processMinutes = false;
  char hours[3];
  char minutes[3];
  int n = 0;
  for (int i = 0; a[i] != '\0'; i++)
  {
    if (!processMinutes)
    {
      hours[n++] = a[i];

      if (a[i] == ':')
      {
	processMinutes = true;
	n = 0;
	continue;
      }
    }
    else
    {
      minutes[n++] = a[i];
    }
  }
  int startMinuteOfDay = atoi(hours) * 60 + atoi(minutes);
  startTimesOfDay.push_back(startMinuteOfDay);
  return true;
}

bool TimerRelay::setStartTime(int a, int b) {
  int startMinuteOfDay = a * 60 + b;
  if (!startTimesOfDay.full())
  {
    startTimesOfDay.push_back(startMinuteOfDay);
    return true;
  }
  return false;
}

void TimerRelay::setEveryOtherDayOn() {
  for (int n = 0; n < 7; n++)
  {
    if (n % 2 == 0)
    {
      runDays[n] = 1;
    }
    else
    {
      runDays[n] = 0;
    }
  }
}

void TimerRelay::setEveryDayOn() {
  for ( int n=0 ; n<7 ; n++ ) {
    runDays[n] = 1;
  }
}

void TimerRelay::setEveryDayOff() {
  for ( int n=0 ; n<7 ; n++ ) {
    runDays[n] = 0;
  }
}

void TimerRelay::setDaysFromArray(Array<int,7> & array) {
  for ( int n=0 ; n<7 ; n++ ) {
    runDays[n] = array[n];
  }
} 

void TimerRelay::setSpecificDayOn(int n) {
  runDays[n] = 1;
}

void TimerRelay::getWeekSchedule(char weekSchedule[8]) {
  for ( int n=0 ; n<7 ; n++ ) {
    if (runDays[n] == 0) { weekSchedule[n] = '_'; }
    else { weekSchedule[n] = 'R'; }
  }
  weekSchedule[7] = '\0';
}

int TimerRelay::checkDayToRun(int thisDay) { 
  return runDays[thisDay];
} 

int TimerRelay::checkDayToRun() { 
  struct tm *timeinfo = localtime(&now);
  int thisDay = timeinfo->tm_wday;
  return runDays[thisDay];
} 

void TimerRelay::checkStartTime(String &timesToStart) {
  for (int i = 0; i < int(startTimesOfDay.size()); i++) {
    if (i != 0) {
      timesToStart += ", ";
    }
    timesToStart += startTimesOfDay[i];
  }
}

bool TimerRelay::isTimeToStart() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  int thisHour = timeinfo->tm_hour;
  int thisMinute = timeinfo->tm_min;
  int thisDay = timeinfo->tm_wday;

  for (int i = 0; i < int(startTimesOfDay.size()); i++) {
    if (runDays[thisDay] && (((thisHour * 60) + thisMinute) == startTimesOfDay[i]) ) {
      // don't run twice in the same minute
      if (now - onTime > 60) return true;
    } 
  }
  return false;
}

bool TimerRelay::isTimeToStop() {
  return now >= onTime + runTime;
}

bool TimerRelay::handle() {
  prevTime = now;
  now = time(nullptr);

  // only check every second
  if ( now == prevTime ) return false;

  //uptime the time left to run every second
  setTimeLeftToRun();
  setNextTimeToRun();

  // if we don't have runtime set, then just return
  if ( initialRunTime == 0 ) return false;

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
    if (!active) return false;
    switchOn();
    return true;
  } 

  return false;
}

/*

//Manage a vector of Irrigation Relays
IrrigationZones::IrrigationZones (int a) { }
IrrigationZones::IrrigationZones (const int ELEMENT_COUNT_MAX, Adafruit_MCP23X17* b) {
  zoneCount = a;

  this::IrrigationRelay * storage_array[ELEMENT_COUNT_MAX];
  Elements.setStorage(storage_array);
}

void IrrigationZones::addZone(const char* name, const char* startTime, int runTime) {

  IrrigationRelay * irz;

  if (isMCP) {
    irz = new IrrigationRelay(name, zoneAddress++, true, startTime, runTime, false, mcp);
  }

  irz->setup();
  IrrigationZones.push_back(irz);
}
*/

// IrrigationRelay constructors
IrrigationRelay::IrrigationRelay (int a): TimerRelay(a) { }
IrrigationRelay::IrrigationRelay (int a, Adafruit_MCP23X17* b): TimerRelay(a, b) { }
//"patio_pots",  7,       true,      "7:00",              3,            , '1111111'
IrrigationRelay::IrrigationRelay (const char* a, int b, bool c, const char* d, int e, bool f, Adafruit_MCP23X17* g): TimerRelay(b, g) { 
  name = new char[strlen(a)+1];
  strcpy(name,a);
  if (c) { this->setBackwards(); }
  this->setStartTimeFromString(d);
  this->setRuntimeMinutes(e);
  if (f) { this->setEveryOtherDayOn(); }
}

// IrrigationRelay methods
// turn on the moisture check at moisturePercentageToRun
void IrrigationRelay::setMoistureSensor(int a, int b) {
  moistureSensor = true;
  moisturePin = a;
  moisturePercentageToRun = b;
}

void IrrigationRelay::setMoistureSensor(uint8_t a, int b, int c) {
  moistureSensor = true;
  i2cMoistureSensor = true;
  ads.begin(a);
  moisturePin = b;
  moisturePercentageToRun = c;
}

void IrrigationRelay::setMoistureLimits(int a, int b) {
  dryMoistureLevel = a;
  wetMoistureLevel = b;
}

void IrrigationRelay::setMoistureLevel(int n) {
  moistureLevel = n;
}

void IrrigationRelay::checkMoisture() {
  double calibratedMoistureLevel = moistureLevel;

  if (i2cMoistureSensor) {
    moistureLevel = ads.readADC_SingleEnded(moisturePin);
  } else {
    moistureLevel = analogRead(moisturePin);
  }

  if (calibratedMoistureLevel > dryMoistureLevel) {
    calibratedMoistureLevel = dryMoistureLevel;
  }
  if (calibratedMoistureLevel < wetMoistureLevel) {
    calibratedMoistureLevel = wetMoistureLevel;
  }

  moisturePercentage = 100 - (((calibratedMoistureLevel - wetMoistureLevel) / (dryMoistureLevel - wetMoistureLevel)) * 100);

  dry = (moisturePercentage < moisturePercentageToRun) ?  true : false;
}

const char* IrrigationRelay::state() {
  if (! dry) return "wet";

  if (on)
  {
    return "on";
  }
  else
  {
    return "off";
  }
}

bool IrrigationRelay::handle() {
  prevTime = now;
  now = time(nullptr);

  // only process the rest every 1 second
  if ( now == prevTime ) return false;

  // uptime the time left to run every second
  setTimeLeftToRun();
  setNextTimeToRun();

  // set the moisture level on every loop
  if (moistureSensor) checkMoisture();

  // if we don't have runtime set, then just return
  if (initialRunTime == 0)  return false;

  // if we're on, turn it off if it's been more than than the time to run or if it's started raining
  // if the scheduleOverride is on, turn it off if the timer expires and resume normal
  // operation
  if (on && isTimeToStop())
  {
    switchOff();
    return true;
  }

  if ( scheduleOverride ) return false;

  // if we're on it started raining, turn it off
  // but ignore the scheduleOveride and stay running if it's set
  if (on && !dry)
  {
    switchOff();
    return true;
  }

  // if we're not on, turn it on if it's the right day and time
  if ( !on && isTimeToStart() ) {
    if (!dry || !active) return false; 
    switchOn();
    return true;
  }

  return false;
}

bool ScheduleRelay::findTime(int hour, int min, int timeArray[4][2] ) {
  for (int i = 0; i <= slotsTaken; i++) {
    if (timeArray[i][hourIndex] == hour && timeArray[i][minuteIndex] == min) {
      return true;
    }
  }
  return false;
}

ScheduleRelay::ScheduleRelay (int a): Relay(a) {}

//on hour, on minute, off hour, off minute
bool ScheduleRelay::setOnOffTimes(int a, int b, int c, int d) {
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

bool ScheduleRelay::handle() {
  prevTime = now;
  now = time(nullptr);
  if ( ( now == prevTime ) || ( now % 5 != 0 ) ) return false;

  if ( scheduleOverride )  return false;

  struct tm *timeinfo = localtime(&now);
  int thisHour = timeinfo->tm_hour;
  int thisMinute = timeinfo->tm_min;
  if ( ! on && findTime(thisHour, thisMinute, onTimes) ) {
    switchOn();
    return true;
  }

  if ( on && findTime(thisHour, thisMinute, offTimes) ) {
    switchOff();
    return true;
  }

  return false;
}

// ScheduleRelay::ScheduleRelay (int a): Relay(a) {}
DuskToDawnScheduleRelay::DuskToDawnScheduleRelay (int a): ScheduleRelay(a) {}

bool DuskToDawnScheduleRelay::setVemlLightSensor() {
  if (veml.setup()) {
    vemlSensor = true;
  } else { 
    vemlSensor = false;
  }
  return vemlSensor;
}

void DuskToDawnScheduleRelay::setNightOffOnHours(int a, int b) {
  nightOffHour = a;
  morningOnHour = b;
}

void DuskToDawnScheduleRelay::setDusk(int a) {
  dusk = a;
}

bool DuskToDawnScheduleRelay::handle() {
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
  int thisHour = timeinfo->tm_hour;
  // Switch on criteria: it's dark, the lights are not on and it's not the middle of the night
  if ( lightLevel < dusk && ! on && !( thisHour >= nightOffHour && thisHour <= morningOnHour ) ) {
    timesLight = 0;
    timesDark++;

    if (timesDark > timesToSample) {
      switchOn();
      return true;
    }
  }

  // Switch off criteria: the lights are on and either it's light or it's in the middle of the night
  if (on && (lightLevel > dusk || ( thisHour >= nightOffHour && thisHour <= morningOnHour ))) {
    timesDark = 0;
    timesLight++;

    if (timesLight > timesToSample) {
      switchOff();
      return true;
    }
  }

  return false;
}

