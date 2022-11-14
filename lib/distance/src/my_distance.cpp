#include "my_distance.h"

DISTANCE::DISTANCE (int a, int b ): echoPin(a), trigPin(b) {}

void DISTANCE::setup(const char* a) {
  name = new char[strlen(a)+1];
  strcpy(name,a);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void DISTANCE::handle() {
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
