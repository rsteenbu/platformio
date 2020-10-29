#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); 

const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
 
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void showNewData() {
    if (newData == true) {
        Serial.print("This just in ... ");
        Serial.println(receivedChars);
        lcd.clear();
        lcd.home();
        lcd.print(receivedChars);
        newData = false;
    }
}

void setup()
{
  Serial.begin(9600);
  lcd.init();                      // initialize the lcd 
  lcd.backlight();
  lcd.setCursor(0,0);
  delay(2000);
  lcd.print("Hello, world!");
}

void loop()
{
  recvWithStartEndMarkers();
  showNewData();
  delay(100);
}
 
