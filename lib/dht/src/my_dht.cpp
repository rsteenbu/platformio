#include "my_dht.h"

//constructors
myDHT::myDHT(uint8_t x, uint8_t y) : DHT_Unified(x, y) {
  pin = x;
  type = y;
  pinMode(pin, INPUT_PULLUP);
};

//member functions
void myDHT::setSensorName(const char* a) {
  sensorName = new char[strlen(a)+1];
  strcpy(sensorName,a);
}

double myDHT::getHumidity() {
  return humid;
}

double myDHT::getTemp() {
  return temp;
}

bool myDHT::handle() {
  this->humidity().getEvent(&event);
  humid = isnan(event.relative_humidity) ? -1 : event.relative_humidity;

  this->temperature().getEvent(&event);
  temp = isnan(event.temperature) ? -1 : event.temperature * 9 / 5 + 32;

  return true;
}
