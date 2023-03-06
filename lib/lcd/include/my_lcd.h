#ifndef MYLCD_H
#define MYLCD_H
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#define LCD_SPACE_SYMBOL 0x20  //space symbol from the LCD ROM, see p.9 of GDM2004D datasheet

class LCD : public LiquidCrystal_I2C {
  public:
    LCD();
    bool state = true;
    void setBackLight(bool a);
};
#endif
