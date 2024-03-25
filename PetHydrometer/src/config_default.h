#pragma once

// 11/6/23 two problems 
// 1) alternating b/w active and inactive based on humidty readings.  maybe because of two sensors?
// 2) it's not wet enough for medusa.

#define DEBUG 0
#define SYSLOG_APP_NAME "pet_sensor"
#define JSON_SIZE 600
#define MYTZ TZ_America_Los_Angeles
#define HTTP_METRICS_ENDPOINT "/metrics"
// Board name
#define BOARD_NAME "ESP8266"
// DHT sensor name (should be the same)
#define DHT_NAME "DHT22"
// DHT sensor type
#define DHT_TYPE DHT11
// DHT pin -> this is not used and should be in the build environment?
#define DHT_PIN D6

#define MEDUSA_MISTER_RUNTIME 30
#define MEDUSA_HUMIDITY_BOUNDRY 20
#define GECKSTER_MISTER_RUNTIME 40
#define GECKSTER_HUMIDITY_BOUNDRY 20

