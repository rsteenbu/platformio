#ifndef MYDISTANCE_H
#define MYDISTANCE_H
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <TZ.h>
#include <Wire.h>

//HC-SR04
 
// Can't use MCP b/c it doesn't support pulseIn
class DISTANCE {
  int echoPin;
  int trigPin;
  Adafruit_MCP23X17* mcp;
  bool i2cPins = false;
  time_t now, prevTime = 0;

  public:
    long inches;
    char* name;

    //constructor
    DISTANCE (int a, int b ): echoPin(a), trigPin(b) {}

    void setup(const char* a) {
      name = new char[strlen(a)+1];
      strcpy(name,a);

      pinMode(trigPin, OUTPUT);
      pinMode(echoPin, INPUT);
    }

   void handle() {
     prevTime = now;
     now = time(nullptr);

     if ( now == prevTime ) return;

     digitalWrite(trigPin, LOW);
     delayMicroseconds(2);
     digitalWrite(trigPin, HIGH);
     delayMicroseconds(10);
     digitalWrite(trigPin, LOW);

     inches = pulseIn(echoPin, HIGH) / 74 / 2;
   }
};
#endif
