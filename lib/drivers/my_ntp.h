#ifndef MYNTP_H
#define MYNTPL_H

#define USE_WIFI_NINA false
#define USE_WIFI_CUSTOM true

class NTP {
  private:

    void sendNTPpacket(IPAddress& timeServer, WiFiUDP& Udp) {
      // set all bytes in the buffer to 0
      memset(packetBuffer, 0, 48);

      // Initialize values needed to form NTP request
      // (see URL above for details on the packets)
      packetBuffer[0] = 0b11100011;   // LI, Version, Mode
      packetBuffer[1] = 0;     // Stratum, or type of clock
      packetBuffer[2] = 6;     // Polling Interval
      packetBuffer[3] = 0xEC;  // Peer Clock Precision
      // 8 bytes of zero for Root Delay & Root Dispersion
      packetBuffer[12]  = 49;
      packetBuffer[13]  = 0x4E;
      packetBuffer[14]  = 49;
      packetBuffer[15]  = 52;

      // all NTP fields have been given values, now
      // you can send a packet requesting a timestamp:
      Udp.beginPacket(timeServer, 123); //NTP requests are to port 123
      Udp.write(packetBuffer, 48);
      Udp.endPacket();
    }

    time_t receiveNTPpacket(WiFiUDP& Udp) {
      if (Udp.parsePacket()) {
	Serial.println(F("Received NTP packet"));
	// We've received a packet, read the data from it
	Udp.read(packetBuffer, 48); // read the packet into the buffer

	//the timestamp starts at byte 40 of the received packet and is four bytes,
	// or two words, long. First, esxtract the two words:

	unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
	unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
	// combine the four bytes (two words) into a long integer
	// this is NTP time (seconds since Jan 1 1900):
	unsigned long secsSince1900 = highWord << 16 | lowWord;

	// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
	const unsigned long seventyYears = 2208988800UL;
	// subtract seventy years:
	return secsSince1900 - seventyYears;

      } else { 
	Serial.println("No UDP Packet received");
	return 0;
      }
    }

  public:
    //constructor
    NTP () {}
    // public variables
    time_t epoch;
    byte packetBuffer[48]; //buffer to hold incoming and outgoing packets

    // public functions
    bool setup(IPAddress& timeServer, WiFiUDP& Udp) {
      sendNTPpacket(timeServer, Udp); // send an NTP packet to a time server
      // wait to see if a reply is available
      delay(1000);
      epoch = receiveNTPpacket(Udp);
      if (epoch == 0) {
	return false;
      }
      return true;
    }
};
#endif
