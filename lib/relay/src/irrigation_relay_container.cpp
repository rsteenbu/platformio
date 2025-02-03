#include "my_relay.h"
#include "irrigation_relay_container.h"

/*

Vector<IrrigationRelay*> IrrigationZones;
IrrigationRelay * storage_array[8];
IrrigationZones.setStorage(storage_array);
IrrigationRelay * irz1 = new IrrigationRelay("patio_pots",  7, true, "7:00",  3, false, &mcp);
*/

IrrigationInstance::IrrigationInstance (const char* a, int b, bool c, const char* d, int e, bool f, Adafruit_MCP23X17* g): TimerRelay(b, g) { 
//IrrigationInstance::IrrigationInstance(int someVal): TimerRelay(someVal) {
  Instances.push_back(this);
}
