#ifndef MYPIR_H
#define MYPIR_H

class PIR {
  int pin;

  public:
    bool pirState;
    PIR (int a ): pin(a) {}

    void setup() {
      pinMode(pin, INPUT);
    }

    bool activity() {
      return pirState;
    }
      
    bool handle() {
       int activity = digitalRead(pin);  // read input value from the motion sensor
       if (activity == HIGH) {
	 pirState = true;
	 return true;
       }
       if (activity == LOW) {
	 pirState = false;
	 return true;
       }
       return false;
    }
};
#endif
