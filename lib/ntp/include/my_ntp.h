#ifndef MYNTP_H
#define MYNTPL_H

#define USE_WIFI_NINA false
#define USE_WIFI_CUSTOM true

#include <WiFiUdp.h>

class NTP {
  private:
    void sendNTPpacket(WiFiUDP& Udp);
    time_t receiveNTPpacket(WiFiUDP& Udp);

  public:
    //constructor
    NTP ();
    // public variables
    time_t epoch;
    byte packetBuffer[48]; //buffer to hold incoming and outgoing packets

    // public functions
    bool setup(WiFiUDP& Udp);
};
#endif
