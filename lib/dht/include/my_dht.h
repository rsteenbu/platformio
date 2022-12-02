#ifndef MYDHT_H
#define MYDHT_H

#include <DHT_U.h>

class myDHT : public DHT_Unified {
  uint8_t pin;
  uint8_t type;

  public:
    //variables
    char* sensorName;
    sensors_event_t event;
    double humid;
    double temp;

    //constructors
    //myDHT(uint8_t x, uint8_t y) : DHT_Unified(x, y);
    myDHT(uint8_t x, uint8_t y);

    void setSensorName(const char* a);
    double getHumidity();
    double getTemp();
    bool handle();
};

#endif

