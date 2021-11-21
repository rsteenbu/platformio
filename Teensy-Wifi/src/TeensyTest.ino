#define USE_WIFI_NINA false
#define USE_WIFI_CUSTOM true
#include <WiFiEspAT.h>
#include <WiFiWebServer.h>
#include <Syslog.h>
#include <ArduinoJson.h>
#include <Timezone.h> 
#include <TimeLib.h>

#include <my_ntp.h>


//const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
//byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

class teensyRelay {
  int onVal = HIGH;
  int offVal = LOW;

  protected:
    int pin;

  public:
    //variables
    bool on = false;
    char* name;

    //constructors
    teensyRelay (int a): pin(a) {}

    int status() {
      if (on) {
	return 1;
      } else {
	return 0;
      }
    }
    void setup(const char* a) {
      name = new char[strlen(a)+1];
      strcpy(name,a);

      pinMode(pin, OUTPUT);
      digitalWrite(pin, offVal); // start off
    }
    void switchOn() {
      if (!on) {
        digitalWrite(pin, onVal);
        on = true;
      }
    }
    void switchOff() {
      if (on) {
        digitalWrite(pin, offVal);
        on = false;
      }
    }
    const char* state() {
      if (on) {
	return "on";
      } else { 
	return "off";
      }
    }
};

teensyRelay * XmasTree = new teensyRelay(22);

WiFiWebServer server(80);
WiFiUdpSender udpClient;

IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
//const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
//byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
NTP ntp;

int debug = 0;
char msg[40];

// This device info
const char* APP_NAME = "system";
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_LOCAL0);

#define JSON_SIZE 200

// US Pacific Time Zone 
TimeChangeRule usPST = {"PST", Second, Sun, Mar, 2, -420};  // Daylight time = UTC - 7 hours
TimeChangeRule usPDT = {"PDT", First, Sun, Nov, 2, -480};   // Standard time = UTC - 8 hours
Timezone usPacific(usPST, usPDT);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

void setup() {
  Serial1.begin(115200);

  XmasTree->setup("xmastree");

  WiFi.init(Serial1);
  WiFi.setHostname(DEVICE_HOSTNAME);
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  // waiting for connection to Wifi network set with the SetupWiFiConnection sketch
  Serial.println("Waiting for connection to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }
  Serial.println();

  // start the http webserver
  server.begin();

  IPAddress ip = WiFi.localIP();
  syslog.logf(LOG_INFO, "Alive! at IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  server.on("/status", handleStatus);
  server.on("/tree", handleTree);

  Udp.begin(2390);
  // wait to see if a reply is available
  delay(1000);

  ntp.setup(timeServer, Udp);

  // Set the system time from the NTP epoch
  setSyncProvider(getTeensy3Time);
  delay(100);
  if (timeStatus()!= timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }

  if (ntp.epoch == 0) {
    Serial.println("Setting NTP time failed");
  } else {
    time_t eastern = usPacific.toLocal(ntp.epoch);
    Teensy3Clock.set(eastern); // set the RTC
    setTime(eastern);
  }
}

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

void handleStatus() {
  StaticJsonDocument<JSON_SIZE> doc;
  JsonObject switches = doc.createNestedObject("switches");
  JsonObject sensors = doc.createNestedObject("sensors");

  switches[XmasTree->name]["state"] = XmasTree->state();
  int lightLevel = 0;
  lightLevel = analogRead(A9);
  sensors["lightLevel"] = lightLevel;
  doc["debug"] = debug;

  // 11/16/21 20:17:07
  char timeString[20];
  sprintf(timeString, "%02d/%02d/%02d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
  doc["time"] = timeString;

  //breakTime(time, &tm);

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

void handleTree() {
  if (server.arg("state") == "status") {
    server.send(200, "text/plain", (XmasTree->on ? "1" : "0"));
  } else if (server.arg("state") == "on") {
    XmasTree->switchOn();
    syslog.logf(LOG_INFO, "Turned %s on", XmasTree->name);
    server.send(200, "text/plain");
  } else if (server.arg("state") == "off") {
    XmasTree->switchOff();
    syslog.logf(LOG_INFO, "Turned %s off", XmasTree->name);
    server.send(200, "text/plain");
  } else {
    server.send(404, "text/plain", "ERROR: uknonwn light command");
  }
}

void loop()
{
  server.handleClient();
}

