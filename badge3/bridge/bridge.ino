#include <Adafruit_NeoPixel.h>                                                                                                                                                                                                                                                      
                                                                                                                                                                                                                                                                                      
  #define PIN        8
  #define NUM_PIXELS 10                                                                                                                                                                                                                                                               
  #define SPACING    6    // LEDs between each active light
                                                                                                                                                                                                                                                                                   
  Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);
                                                                                                                                                                                                                                                                                      
  float offset = 0.0;
  const float SPEED = 0.05;
                                                                                                                                                                                                                                                                                      
  void setup() {
    strip.begin();                                                                                                                                                                                                                                                                    
    strip.show(); 
  }

  void loop() {
    for (int i = 0; i < NUM_PIXELS; i++) {
      // Distance from pixel i to nearest active light (wraps seamlessly)
      float phase = fmod(i - offset + NUM_PIXELS * SPACING, (float)SPACING);                                                                                                                                                                                                          
      float dist = min(phase, (float)SPACING - phase);                                                                                                                                                                                                                                
                                                                                                                                                                                                                                                                                      
      int brightness;                                                                                                                                                                                                                                                                 
      if      (dist < 0.5) brightness = 60;  // active peak
      else if (dist < 1.5) brightness = 30;   // ±1 neighbor                                                                                                                                                                                                                          
      else if (dist < 2.5) brightness = 15;   // ±2 neighbor
      else                  brightness = 7;   // baseline (always on)                                                                                                                                                                                                                
                  
      strip.setPixelColor(i, strip.Color(brightness, 0, 0));                                                                                                                                                                                                                          
    }             
                                                                                                                                                                                                                                                                            
    strip.show(); 
    delay(10);

    offset += SPEED;
    if (offset >= SPACING) offset -= SPACING;
  }
