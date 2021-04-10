#ifndef MYVEML_H
#define MYVEML_H
#include <Adafruit_VEML7700.h>

class Veml {
  Adafruit_VEML7700 veml = Adafruit_VEML7700();

  public:
    //constructor
    Veml () {}

    // public variables
    int integrationTime = 0;
    double gain = 0;

    // public functions
    bool setup() {
      bool initialized = false;

      if (veml.begin()) {
	veml.setGain(VEML7700_GAIN_1);
	veml.setIntegrationTime(VEML7700_IT_800MS);

	switch (veml.getGain()) {
	  case VEML7700_GAIN_1: gain = 1; break;
	  case VEML7700_GAIN_2: gain = 2; break;
	  case VEML7700_GAIN_1_4: gain = .25; break;
	  case VEML7700_GAIN_1_8: gain = .125; break;
	}

	Serial.print(F("Integration Time (ms): "));
	switch (veml.getIntegrationTime()) {
	  case VEML7700_IT_25MS: integrationTime = 25; break;
	  case VEML7700_IT_50MS: integrationTime = 50; break;
	  case VEML7700_IT_100MS: integrationTime = 100; break;
	  case VEML7700_IT_200MS: integrationTime = 200; break;
	  case VEML7700_IT_400MS: integrationTime = 400; break;
	  case VEML7700_IT_800MS: integrationTime = 800; break;
	}
	veml.setLowThreshold(10000);
	veml.setHighThreshold(20000);
	veml.interruptEnable(false);

	initialized = true;
      }

      return initialized;
    }

    int readLux() {
      return veml.readLux();
    }
};
#endif
