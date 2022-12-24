#ifndef TWINKLEFOX_H
#define TWINKLEFOX_H
#include "FastLED.h"

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries 
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 0
// If COOL_LIKE_INCANDESCENT is set to 1, colors will 
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1
// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).  
// 4, 5, and 6 are recommended, default is 4.
#define TWINKLE_SPEED 4
// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).
// Default is 5.
#define TWINKLE_DENSITY 5

// How often to change color palettes.
#define SECONDS_PER_PALETTE  30
// Background color for 'unlit' pixels
// Can be set to CRGB::Black if desired.
CRGB gBackgroundColor = CRGB::Black;
// Example of dim incandescent fairy light background color
// CRGB gBackgroundColor = CRGB(CRGB::FairyLight).nscale8_video(16);

#define NUM_LEDS      300
#define DATA_PIN        5
#define VOLTS          12
#define MAX_MA       4000

CRGBArray<NUM_LEDS> leds;
CRGBPalette16 gCurrentPalette;
CRGBPalette16 gTargetPalette;


class TwinkleFox {
  public:
    //constructors
    TwinkleFox ();

    // A mostly red palette with green accents and white trim.
    // "CRGB::Gray" is used as white to keep the brightness more uniform.
    const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM =
    {  CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
       CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
       CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray, 
       CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green };

    // A mostly (dark) green palette with red berries.
    #define Holly_Green 0x00580c
    #define Holly_Red   0xB00402
    const TProgmemRGBPalette16 Holly_p FL_PROGMEM =
    {  Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
       Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
       Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
       Holly_Green, Holly_Green, Holly_Green, Holly_Red 
    };

    // A red and white striped palette
    // "CRGB::Gray" is used as white to keep the brightness more uniform.
    const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM =
    {  CRGB::Red,  CRGB::Red,  CRGB::Red,  CRGB::Red, 
       CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray,
       CRGB::Red,  CRGB::Red,  CRGB::Red,  CRGB::Red, 
       CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray };

    // A mostly blue palette with white accents.
    // "CRGB::Gray" is used as white to keep the brightness more uniform.
    const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM =
    {  CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
       CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
       CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
       CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray };

    // A pure "fairy light" palette with some brightness variations
    #define HALFFAIRY ((CRGB::FairyLight & 0xFEFEFE) / 2)
    #define QUARTERFAIRY ((CRGB::FairyLight & 0xFCFCFC) / 4)
    const TProgmemRGBPalette16 FairyLight_p FL_PROGMEM =
    {  CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, 
       HALFFAIRY,        HALFFAIRY,        CRGB::FairyLight, CRGB::FairyLight, 
       QUARTERFAIRY,     QUARTERFAIRY,     CRGB::FairyLight, CRGB::FairyLight, 
       CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight };

    // A palette of soft snowflakes with the occasional bright one
    const TProgmemRGBPalette16 Snow_p FL_PROGMEM =
    {  0x304048, 0x304048, 0x304048, 0x304048,
       0x304048, 0x304048, 0x304048, 0x304048,
       0x304048, 0x304048, 0x304048, 0x304048,
       0x304048, 0x304048, 0x304048, 0xE0F0FF };

    // A palette reminiscent of large 'old-school' C9-size tree lights
    // in the five classic colors: red, orange, green, blue, and white.
    #define C9_Red    0xB80400
    #define C9_Orange 0x902C02
    #define C9_Green  0x046002
    #define C9_Blue   0x070758
    #define C9_White  0x606820
    const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM =
    {  C9_Red,    C9_Orange, C9_Red,    C9_Orange,
       C9_Orange, C9_Red,    C9_Orange, C9_Red,
       C9_Green,  C9_Green,  C9_Green,  C9_Green,
       C9_Blue,   C9_Blue,   C9_Blue,
       C9_White
    };

    // A cold, icy pale blue palette
    #define Ice_Blue1 0x0C1040
    #define Ice_Blue2 0x182080
    #define Ice_Blue3 0x5080C0
    const TProgmemRGBPalette16 Ice_p FL_PROGMEM =
    {
      Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
      Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
      Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
      Ice_Blue2, Ice_Blue2, Ice_Blue2, Ice_Blue3
    };

    // Add or remove palette names from this list to control which color
    // palettes are used, and in what order.
    const TProgmemRGBPalette16* ActivePaletteList[] = {
      &RetroC9_p,
      &BlueWhite_p,
      &RainbowColors_p,
      &FairyLight_p,
      &RedGreenWhite_p,
      &PartyColors_p,
      &RedWhite_p,
      &Snow_p,
      &Holly_p,
      &Ice_p  
    };

    // Advance to the next color palette in the list (above).
    void chooseNextColorPalette( CRGBPalette16& pal);

    //  This function loops over each pixel, calculates the 
    //  adjusted 'clock' that this pixel should use, and calls 
    //  "CalculateOneTwinkle" on each pixel.  It then displays
    //  either the twinkle color of the background color, 
    //  whichever is brighter.
    void drawTwinkles( CRGBSet& L);

    //  This function takes a time in pseudo-milliseconds,
    //  figures out brightness = f( time ), and also hue = f( time )
    //  The 'low digits' of the millisecond time are used as 
    //  input to the brightness wave function.  
    //  The 'high digits' are used to select a color, so that the color
    //  does not change over the course of the fade-in, fade-out
    //  of one cycle of the brightness wave function.
    //  The 'high digits' are also used to determine whether this pixel
    //  should light at all during this cycle, based on the TWINKLE_DENSITY.
    CRGB computeOneTwinkle( uint32_t ms, uint8_t salt);

    // This function is like 'triwave8', which produces a 
    // symmetrical up-and-down triangle sawtooth waveform, except that this
    // function produces a triangle wave with a faster attack and a slower decay:
    //
    //     / \ 
    //    /     \ 
    //   /         \ 
    //  /             \ 
    //
    uint8_t attackDecayWave8( uint8_t i);

    // This function takes a pixel, and if its in the 'fading down'
    // part of the cycle, it adjusts the color a little bit like the 
    // way that incandescent bulbs fade toward 'red' as they dim.
    void coolLikeIncandescent( CRGB& c, uint8_t phase);

};

#endif
