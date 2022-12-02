#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <Vector.h>

#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>

#include <Wire.h>
#include <my_lcd.h>
#include <my_relay.h>
#include <my_motion.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// Syslog server connection info

#define APP_NAME "base"
#define JSON_SIZE 600
#define MYTZ TZ_America_Los_Angeles

ESP8266WebServer server(80);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

class myDHT : public DHT_Unified {
  uint8_t pin;
  uint8_t type;

  public:
    char* sensorName;
    sensors_event_t event;
    double humid;
    double temp;

    //constructor
    myDHT(uint8_t x, uint8_t y) : DHT_Unified(x, y) {
      pin = x;
      type = y;
      pinMode(pin, INPUT_PULLUP);
    };
    
    //member functions
    void setSensorName(const char* a) {
      sensorName = new char[strlen(a)+1];
      strcpy(sensorName,a);
    }

    double getHumidity() {
      this->humidity().getEvent(&event);
      humid = isnan(event.relative_humidity) ? -1 : event.relative_humidity;
      return humid;
    }
    double getTemp() {
      this->temperature().getEvent(&event);
      temp = isnan(event.temperature) ? -1 : event.temperature * 9 / 5 + 32;
      return temp;
    }
};

Vector<myDHT*> DHTSensors;
myDHT* storage_array[8];


int debug = 0;
char msg[40];
LCD * lcd = new LCD();
MOTION * motion = new MOTION(D7); 
TimerRelay * Mister = new TimerRelay(D3);

void handleDebug() {
  if (server.arg("level") == "status") {
    char msg[40];
    sprintf(msg, "Debug level: %d", debug);
    server.send(200, "text/plain", msg);
  } else if (server.arg("level") == "0") {
    syslog.log(LOG_INFO, "Debug level 0");
    debug = 0;
    server.send(200, "text/plain");
  } else if (server.arg("level") == "1") {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
    server.send(200, "text/plain");
  } else if (server.arg("level") == "2") {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown debug command");
  }
}

void handleMister() {
  if (server.arg("state") == "on") {
    Mister->switchOn();
    syslog.logf(LOG_INFO, "Turned %s on for %ds", Mister->name, Mister->runTime);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    Mister->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off", Mister->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "status") {
    server.send(200, "text/plain", Mister->on ? "1" : "0");
  } else if (server.arg("addTime") != "") {
    int timeToAdd = server.arg("addTime").toInt();
    Mister->addTimeToRun(timeToAdd);
    syslog.logf(LOG_INFO, "Added %d seconds, total runtime now %ds, time left %ds", timeToAdd, Mister->runTime, Mister->getSecondsLeft());
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: unknown mister command");
  }
}

void handleDisplay() {
  if (server.arg("state") == "on") {
    server.send(200, "text/plain");
    lcd->setBackLight(true);
    syslog.log(LOG_INFO, "Turned LCD Display on");
  } else if (server.arg("state") == "off") {
    server.send(200, "text/plain");
    lcd->setBackLight(false);
    syslog.log(LOG_INFO, "Turned LCD Display off");
  } else {
    server.send(404, "text/plain", "ERROR: unknown scani2c command");
  }
}

void handleSensors() {
  for (myDHT* sensor : DHTSensors) {
    char msg[10];
    if (server.arg("temp") == "left") {
      if (strcmp(sensor->sensorName, "leftDHT") == 0) {
	sprintf(msg, "%0.2f", sensor->temp);
	server.send(200, "text/plain", msg);
      }
    } else if (server.arg("temp") == "right") {
      if (strcmp(sensor->sensorName, "rightDHT") == 0) {
	sprintf(msg, "%0.2f", sensor->temp);
	server.send(200, "text/plain", msg);
      }
    } else if (server.arg("humidity") == "left") {
      if (strcmp(sensor->sensorName, "leftDHT") == 0) {
	sprintf(msg, "%0.2f", sensor->humid);
	server.send(200, "text/plain", msg);
      }
    } else if (server.arg("humidity") == "right") {
      if (strcmp(sensor->sensorName, "rightDHT") == 0) {
	sprintf(msg, "%0.2f", sensor->humid);
	server.send(200, "text/plain", msg);
      }
    } else {
      server.send(404, "text/plain", "ERROR: Sensor not found.");
    }
  }
}

void handleStatus() {
  time_t now;
  now = time(nullptr);
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");

  switches["mister"]["state"] = Mister->state();
  switches["mister"]["Time Left"] = Mister->timeLeftToRun;
  JsonObject sensors = doc.createNestedObject("sensors");

  for (myDHT* sensor : DHTSensors) {
    sensors[sensor->sensorName]["humidity"] = sensor->humid;
    sensors[sensor->sensorName]["temperature"] = sensor->temp;
  }

  doc["LCD Backlight Status"] = lcd->state;
  doc["debug"] = debug;

  char timeString[20];
  struct tm *timeinfo = localtime(&now);
  strftime (timeString,20,"%D %T",timeinfo);
  doc["time"] = timeString;

  size_t jsonDocSize = measureJsonPretty(doc);
  if (jsonDocSize > JSON_SIZE) {
    char msg[40];
    sprintf(msg, "ERROR: JSON message too long, %d", jsonDocSize);
    server.send(500, "text/plain", msg);
  } else {
    String httpResponse;
    serializeJsonPretty(doc, httpResponse);
    server.send(500, "text/plain", httpResponse);
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Booting up");

  // Setup the Relay
  Mister->setup("misting_system");
  Mister->setRuntime(10);

  // set I2C pins (SDA, CLK)
  Wire.begin(D2, D1);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to network...");
    delay(500);
  }

  sprintf(msg, "%s Alive! at IP: %s", DEVICE_HOSTNAME, (char*) WiFi.localIP().toString().c_str());
  Serial.println(msg);
  syslog.logf(LOG_INFO, msg);

  // Setup OTA Update
  ArduinoOTA.begin();

  configTime(MYTZ, "pool.ntp.org");

  // Start the server
  server.on("/debug", handleDebug);
  server.on("/mister", handleMister);
  server.on("/display", handleDisplay);
  server.on("/sensors", handleSensors);
  server.on("/status", handleStatus);
  server.begin();

  if (!lcd->begin(16, 2))  {
    syslog.log(LOG_INFO, "LCD Initialization failed");
  }

  DHTSensors.setStorage(storage_array);
  
  if (strcmp(DEVICE_HOSTNAME, "iot-medusa") == 0) {
    myDHT* dhtLeft = new myDHT(D5, DHT22);
    dhtLeft->begin();
    dhtLeft->setSensorName("leftDHT");
    DHTSensors.push_back(dhtLeft);

    myDHT* dhtRight = new myDHT(D6, DHT22);
    dhtRight->begin();
    dhtRight->setSensorName("rightDHT");
    DHTSensors.push_back(dhtRight);
  } else if (strcmp(DEVICE_HOSTNAME, "iot-geckster") == 0) {
    myDHT* dht = new myDHT(D5, DHT22);
    dht->begin();
    dht->setSensorName("rightDHT");
    DHTSensors.push_back(dht);
  }
}

time_t prevTime = 0;
time_t now = 0;
bool misterRan = false;

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  prevTime = now;
  now = time(nullptr);

  motion->handle();
  if (motion->activity() && ! lcd->state) {
    lcd->setBackLight(true);
    syslog.log(LOG_INFO, "Person detected, turned backlight on");
  } 
  if (!motion->activity() && lcd->state) {
    lcd->setBackLight(false);
    syslog.log(LOG_INFO, "Nobody detected, turned backlight off");
  }

  Mister->handle();

  if ( now != prevTime ) {
    lcd->setCursor(0, 0);
    if ( Mister->status() ) {
      char row1[17];
      sprintf(row1, "Time left: %s", Mister->timeLeftToRun); 
      lcd->print(row1);
      misterRan = true;
    } else if ( now % 2 == 0 ) { // only pull from the sensor every 2 secs
      //clear the screen if the mister just finished running
      if (misterRan) { 
	lcd->clear();
	misterRan=false;
      }
      lcd->setCursor(0, 0);
      lcd->print("H:");
      for (myDHT* sensor : DHTSensors) {
	char row1[10];
	sprintf(row1, " %4.2f%%", sensor->getHumidity());
	lcd->print(row1);
      }
      // print the temperature on the 2nd row
      lcd->setCursor(0, 1);
      lcd->print("T:");
      for (myDHT* sensor : DHTSensors) {
	char row2[10];
	sprintf(row2, " %4.2f%%", sensor->getTemp());
	lcd->print(row2);
      }
    }
  }
}
