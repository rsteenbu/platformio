#pragma once

#define DEBUG 0
#define JSON_SIZE 600
#define MYTZ TZ_America_Los_Angeles
#define HTTP_METRICS_ENDPOINT "/metrics"
// Board name
#define BOARD_NAME "ESP8266"

#define SYSTEM_APPNAME "iot"
#define LIGHT_APPNAME "lightsensor"
#define THERMO_APPNAME "thermometer"
#define MOTION_APPNAME "motionsensor"
#define LIGHTSWITCH_APPNAME "lightswitch"
#define IRRIGATION_APPNAME "irrigation"

#ifdef ARDUINO_ESP8266_WEMOS_D1MINI  // WeMos mini and D1 R2
  #define STATUS_LED 5
  #define MOISTURE_PIN A0
  #define ONEWIRE_PIN D7
#elif ARDUINO_ESP8266_ESP01           // Generic ESP's use for 01's
  #define TX_PIN 1
  #define RX_PIN 3
  #define GPIO0_PIN 0
  #define GPIO2_PIN 2
  #define REED_PIN 15
#endif

