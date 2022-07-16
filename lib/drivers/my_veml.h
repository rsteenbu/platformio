#ifndef MYVEML_H
#define MYVEML_H
#include <Adafruit_VEML7700.h>

class Veml {
  Adafruit_VEML7700 veml = Adafruit_VEML7700();

  public:
    //constructor
    Veml ();

    // public variables
    int integrationTime = 0;
    double gain = 0;
    bool initialized = false;

    // public functions
    bool setup();
    int readLux();
};
#endif
