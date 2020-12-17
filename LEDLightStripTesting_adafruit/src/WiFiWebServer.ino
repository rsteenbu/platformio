#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Syslog.h>
#include <ezTime.h>

#include <NeoPixelBus.h>
const uint16_t PixelCount = 300; // this example assumes 4 pixels, making it smaller will cause a failure
const uint8_t PixelPin = 3;  // make sure to set this to the correct pin, ignored for Esp8266
#define colorSaturation 128
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

HslColor hslRed(red);
HslColor hslGreen(green);
HslColor hslBlue(blue);
HslColor hslWhite(white);
HslColor hslBlack(black);


const char* SYSLOG_SERVER = "ardupi4";
const int SYSLOG_PORT = 514;
const char* DEVICE_HOSTNAME = "iot-ledstrip";
const char* APP_NAME = "ledstrip";
WiFiServer server(80);

WiFiUDP udpClient; // A UDP instance to let us send and receive packets over UDP

// Create a new syslog instance with LOG_LOCAL0 facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

Timezone myTZ;
int debug;

class uptimeDisplay {
  private:
    int seconds;
  public:
    uptimeDisplay() {
      setSeconds();
      uptimeDays();
      uptimeHours();
      uptimeMinutes();
      uptimeSeconds();
    }

    void setSeconds() {
       seconds = (int) millis() / 1000;
    }
    int uptimeDays() {
       return seconds / (60*60*24);
    }
    int uptimeHours() {
       return (seconds % (60*60*24)) / (60*60);
    }
    int uptimeMinutes() {
       return ((seconds % (60*60*24)) % (60*60)) / 60;
    }
    int uptimeSeconds() {
       return seconds % 60;
    }
};
uptimeDisplay currUptimeDisplay;

#define NUM_LEDS 300
#define DATA2_PIN 14

void setup()
{
    pinMode(DATA2_PIN, OUTPUT);
    Serial.begin(9600);
    // Giving it a little time because the serial monitor doesn't
    // immediately attach. Want the firmware that's running to
    // appear on each upload.
    delay(2000);

    Serial.println();
    Serial.println("Running Firmware.");

    // Connect to Wifi.
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.print(WIFI_SSID);
    Serial.print(" with password: ");
    Serial.println(WIFI_PASS);

    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    //WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("Connecting...");

    while (WiFi.status() != WL_CONNECTED) {
      // Check to see if connecting failed.
      // This is due to incorrect credentials
      if (WiFi.status() == WL_CONNECT_FAILED) {
        Serial.println("Failed to connect to WIFI. Please verify credentials: ");
        Serial.println();
        Serial.print("SSID: ");
        Serial.println(WIFI_SSID);
        Serial.print("Password: ");
        Serial.println(WIFI_PASS);
        Serial.println();
      }
      delay(5000);
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("Hello World, I'm connected to the internets!!");
    syslog.logf(LOG_INFO, "Alive! at IP: %s", (char*) WiFi.localIP().toString().c_str());

  // Setup OTA Update
  ArduinoOTA.begin();

  // Start the server
  server.begin();
  Serial.println(F("Server started"));
  //Set the time
  waitForSync();
  myTZ.setLocation(F("America/Los_Angeles"));

    // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();

}

int i = 0;
int trips = 0;

void loop()
{
  ArduinoOTA.handle();

  // call the eztime events to update ntp date when it wants
  events();

  delay(500);

    // set the colors, 
    // if they don't match in order, you need to use NeoGrbFeature feature
    strip.SetPixelColor(0, red);
    strip.SetPixelColor(1, green);
    strip.SetPixelColor(2, blue);
    strip.SetPixelColor(3, white);
    // the following line demonstrates rgbw color support
    // if the NeoPixels are rgbw types the following line will compile
    // if the NeoPixels are anything else, the following line will give an error
    //strip.SetPixelColor(3, RgbwColor(colorSaturation));
    strip.Show();

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // setup a new client
  client.setTimeout(5000); // default is 1000

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  
  delayMicroseconds(50);

  // Tell the client we're ok
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  // Check the request and determine what to do with the switch relay
  if (req.indexOf(F("/debug/0")) != -1) {
    syslog.log(LOG_INFO, "Setting debug level: 0");
    debug = 0;
  } else if (req.indexOf(F("/debug/1")) != -1) {
    syslog.log(LOG_INFO, "Setting debug level 1");
    debug = 1;
  } else if (req.indexOf(F("/debug/2")) != -1) {
    syslog.log(LOG_INFO, "Setting debug level 2");
    debug = 2;
  } else if (req.indexOf(F("/status")) != -1) {
    client.printf("{\"uptime\": \"%d:%02d:%02d:%02d\", \"debug\": \"%d\"}\n", 
         currUptimeDisplay.uptimeDays(), currUptimeDisplay.uptimeHours(), currUptimeDisplay.uptimeMinutes(), currUptimeDisplay.uptimeSeconds(), debug);
  } else {
      syslog.logf("received invalid request: %s", req.c_str());
  } 
}

