// NeoPixel Ring simple sketch (c) 2013 Shae Erisson
// released under the GPLv3 license to match the rest of the AdaFruit NeoPixel library

//Random-Pixels is based upon the work of Shae Erisson's Neopixel Ring simple sketch

#include <Adafruit_NeoPixel.h>

long randNumber;

#define PIN_6          6
#define NUMPIXELS_8    8

#define PIN_5          5
#define NUMPIXELS_4    4

#define PIN_3          3
#define NUMPIXELS_3    3


//Adafruit_NeoPixel pixels1 = Adafruit_NeoPixel(PIN_6, NUMPIXELS_8, NEO_GRB + NEO_KHZ800);
//Adafruit_NeoPixel pixels2 = Adafruit_NeoPixel(PIN_5, NUMPIXELS_4, NEO_GRB + NEO_KHZ800);
//Adafruit_NeoPixel pixels3 = Adafruit_NeoPixel(PIN_3, NUMPIXELS_3, NEO_GRB + NEO_KHZ800);

Adafruit_NeoPixel strip1(NUMPIXELS_8, PIN_6, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUMPIXELS_4, PIN_5, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(NUMPIXELS_3, PIN_3, NEO_GRB + NEO_KHZ800);

int delayval = 100;

void setup() {
  strip1.begin();
  strip2.begin();
  strip3.begin();
 
  randomSeed(analogRead(0));
}

void loop() {
  int r = random(128);
  int g = random(128);
  int b = random(128);

  for(int i=0;i<NUMPIXELS_8;i++) {
      strip1.setPixelColor(i, strip1.Color(r,g,b));
      strip1.show(); // This sends the updated pixel color to the hardware.
      delay(delayval); // Delay for a period of time (in milliseconds).
  }
  
  for(int i=0;i<NUMPIXELS_4;i++) {
      strip2.setPixelColor(i, strip2.Color(r,g,b));
      strip2.show(); // This sends the updated pixel color to the hardware.
      delay(delayval); // Delay for a period of time (in milliseconds).
  }
  
  for(int i=0;i<NUMPIXELS_3;i++) {
      strip3.setPixelColor(i, strip3.Color(r,g,b));
      strip3.show(); // This sends the updated pixel color to the hardware.
      delay(delayval); // Delay for a period of time (in milliseconds).
  }
}
