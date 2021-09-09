#ifndef MYLCD_H
#define MYLCD_H
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define LCD_SPACE_SYMBOL 0x20  //space symbol from the LCD ROM, see p.9 of GDM2004D datasheet

class LCD : public LiquidCrystal_I2C {
  public:
    LCD() : LiquidCrystal_I2C(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE) {}

    bool state = true;

    void setBackLight(bool a){
       state = a;
       if (a) { this->backlight(); } 
       else { this->noBacklight(); }
    }

};
#endif
