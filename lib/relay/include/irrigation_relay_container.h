#ifndef IRRIGATION_CONTAINER_H
#define IRRIGATION_CONTAINER_H

#include <Vector.h>

class IrrigationInstance: public TimerRelay {
  private:

  public:
    int val;
    static Vector<IrrigationInstance*> Instances;

    //constructor
    IrrigationInstance(int someVal);
    IrrigationInstance(const char* a, int b, bool c, const char* d, int e, bool f, Adafruit_MCP23X17* g);
    
};

#endif
