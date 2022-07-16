#include "my_lcd.h"

//constructor
LCD::LCD() : LiquidCrystal_I2C(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE) {}

void LCD::setBackLight(bool a) {
  state = a;
  if (a) { this->backlight(); } 
  else { this->noBacklight(); }
}
