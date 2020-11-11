#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include <ArduinoOTA.h>
#include <ezTime.h>
#include <Adafruit_ADS1015.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#include <my_relay.h>

// Create an instance of the server
// specify the port to listen on as an argument
const char* ssid = "***REMOVED***";
const char* password = "***REMOVED***";
WiFiServer server(80);

// This device info
const char* DEVICE_HOSTNAME =  "iot-backyardlight";
const char* APP_NAME = "light";
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;
// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, "ardupi4", 514, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

int debug = 0;
Timezone myTZ;
// Adafruit i2c analog interface
Adafruit_ADS1115 ads(0x48);

// ESP-01 Pins
const int TX_PIN=1;
const int RX_PIN=3;
const int GPIO0_PIN=0;
const int GPIO2_PIN=2;

const int DHTPIN = GPIO0_PIN;

// Initialize DHT on TX pin 1
DHT dht(DHTPIN, DHT22);

Relay lvLights(GPIO2_PIN);   // (ESP-01) TX 

void setup() {
  Serial.begin(115200);

  // prepare LED
  lvLights.setup();

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_HOSTNAME);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  syslog.logf(LOG_INFO, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());

  // Setup OTA Update
  ArduinoOTA.begin();

  //Set the time and timezone
  waitForSync();
  myTZ.setLocation(F("America/Los_Angeles"));
  myTZ.setDefault();

  // set I2C pins (SDA = GPIO2, SDL = GPIO0)
  // set I2C pins (SDA = TX, SDL = GPIO0)
  Wire.begin(TX_PIN /*sda*/, RX_PIN /*sdl*/);

  // Start the server
  server.begin();
  
  // Start the i2c analog gateway
  ads.begin();

//  digitalWrite(DHTPIN, LOW); // sets output to gnd
//  pinMode(DHTPIN, OUTPUT); // switches power to DHT on
//  delay(1000); // delay necessary after power up for DHT to stabilize
  // prepare DHTPIN
  pinMode(DHTPIN, FUNCTION_3);
  dht.begin();
}

int16_t lightLevel = 0;
int const dusk = 500;
bool lvLightOverride = false;
float humidity;
float temperature;
void loop() {
  ArduinoOTA.handle();

  if (secondChanged()) {
    if (debug == 2) {
      syslog.log(LOG_INFO, "DEBUG: In secondChanged loop");
    }
    lightLevel = ads.readADC_SingleEnded(0);
    humidity = dht.readHumidity();
    temperature = dht.readTemperature(true); // Read temperature as Fahrenheit (isFahrenheit = true)
  } 

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // setup a new client
  client.setTimeout(5000); // default is 1000

  // Read the first line of the request
  String req = client.readStringUntil('\r');

  // Tell the client we're ok
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  // Check the request and determine what to do with the switch relay
  if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Turning debug off");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Debug level 2");
    debug = 2;
  } else if (req.indexOf(F("/light/status")) != -1) {
    client.print(lvLights.on);
  } else if (req.indexOf(F("/light/off")) != -1) {
      syslog.log(LOG_INFO, "Turning light off");
      lvLights.switchOff();
  } else if (req.indexOf(F("/light/on")) != -1) {
      syslog.log(LOG_INFO, "Turning light on");
      lvLights.switchOn();
  } else if (req.indexOf(F("/status")) != -1) {
    client.printf("{\"switches\": {\"lvlight\": {\"state\": \"%s\", \"override\": \"%s\"}}, \"sensors\": {\"light\": %d, \"temperature\": %.2f, \"humidity\": %.2f}, \"debug\": %d}\n", 
				lvLights.state(), lvLightOverride ? "on" : "off", lightLevel, temperature, humidity, debug);
  } else {
    syslog.log("received invalid request");
  }

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  // The client will actually be *flushed* then disconnected
  // when the function returns and 'client' object is destroyed (out-of-scope)
  // flush = ensure written data are received by the other side
}
