#ifndef IRRIGATION_CONFIG_H
#define IRRIGATION_CONFIG_H

#include "Adafruit_MCP23X17.h"
#include <my_relay.h>
#include <Vector.h>

// Irrigation zone configuration structure
struct IrrigationZoneConfig {
  const char* name;
  uint8_t pin;
  bool backwards;
  const char* startTime;
  uint8_t durationMinutes;
  bool everyOtherDay;
};

// Irrigation zone configurations
// Edit this array to add, remove, or modify irrigation zones
const IrrigationZoneConfig IRRIGATION_ZONES[] = {
  // name,          pin, backwards, startTime, duration, everyOtherDay
  {"patio_pots",      7,     true,    "7:00",        3,        false},
  {"cottage",         6,     true,    "7:15",       15,         true},
  {"south_fence",     5,     true,    "7:30",        5,        false},
  {"hill",            4,     true,    "7:45",       20,        false},
  {"unused_0",        3,     true,    "8:15",        5,        false},
  {"back_fence",      2,     true,    "8:15",       15,        false},
  {"north_fence",     1,     true,    "8:30",       15,         true},
  {"unused_1",        0,     true,    "8:30",        5,         true}
};

// Number of irrigation zones (calculated automatically)
#define NUM_IRRIGATION_ZONES (sizeof(IRRIGATION_ZONES) / sizeof(IrrigationZoneConfig))

// Initialize all irrigation zones from configuration
// Parameters:
//   zones - Vector to store irrigation zone pointers
//   mcp - Pointer to the MCP23X17 GPIO expander
void setupIrrigationZones(Vector<IrrigationRelay*>& zones, Adafruit_MCP23X17* mcp) {
  for (size_t i = 0; i < NUM_IRRIGATION_ZONES; i++) {
    const IrrigationZoneConfig& config = IRRIGATION_ZONES[i];

    IrrigationRelay* zone = new IrrigationRelay(
      config.name,
      config.pin,
      config.backwards,
      config.startTime,
      config.durationMinutes,
      config.everyOtherDay,
      mcp
    );

    zone->setup();
    zones.push_back(zone);
  }
}

#endif // IRRIGATION_CONFIG_H
