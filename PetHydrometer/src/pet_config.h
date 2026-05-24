#ifndef PET_CONFIG_H
#define PET_CONFIG_H

#include <my_relay.h>
#include <my_dht.h>
#include <Vector.h>

struct DHTSensorConfig {
  const char* name;
  uint8_t pin;
  uint8_t type;
};

struct PetConfig {
  const char* hostname;
  const char* misterName;
  int misterRuntime;
  int humidityBoundary;
  const char* startTimes[4];
  uint8_t numStartTimes;
  DHTSensorConfig sensors[2];
  uint8_t numSensors;
};

const PetConfig PET_CONFIGS[] = {
  // hostname,      misterName,         runtime, boundary, startTimes,                          #times, sensors,                                          #sensors
  {"iot-medusa",   "misting_system",    30,      70,       {"8:00", "12:00", "16:00", "20:00"}, 4,      {{"leftDHT", D5, DHT22}, {"rightDHT", D6, DHT22}}, 2},
  {"iot-geckster", "misting_system",    30,      50,       {"8:00", "12:00", "16:00", "20:00"}, 4,      {{"DHT",     D5, DHT22}},                          1},
};

#define NUM_PET_CONFIGS (sizeof(PET_CONFIGS) / sizeof(PetConfig))

void setupPet(IrrigationRelay* mister, Vector<myDHT*>& sensors) {
  for (size_t i = 0; i < NUM_PET_CONFIGS; i++) {
    const PetConfig& config = PET_CONFIGS[i];
    if (strcmp(DEVICE_HOSTNAME, config.hostname) != 0) continue;

    mister->setup(config.misterName);
    mister->setEveryDayOn();
    for (uint8_t t = 0; t < config.numStartTimes; t++) {
      mister->setStartTimeFromString(config.startTimes[t]);
    }
    mister->setRuntime(config.misterRuntime);
    mister->setMoistureLevel(config.humidityBoundary);

    for (uint8_t s = 0; s < config.numSensors; s++) {
      myDHT* dht = new myDHT(config.sensors[s].pin, config.sensors[s].type);
      dht->begin();
      dht->setSensorName(config.sensors[s].name);
      sensors.push_back(dht);
    }
    return;
  }
}

#endif // PET_CONFIG_H
