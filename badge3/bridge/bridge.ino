#include <Adafruit_NeoPixel.h>

#define PIN        8
#define NUM_PIXELS 10
#define SPACING    6    // LEDs between each active light

#define PIN2        9
#define NUM_PIXELS2 6

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_PIXELS2, PIN2, NEO_GRB + NEO_KHZ800);

float offset = 0.0;
const float SPEED = 0.05;

uint16_t rainbowHue = 0;

void setup() {
  strip.begin();
  strip.show();
  strip2.begin();
  strip2.show();
}

void loop() {
  // --- Strip 1: traveling red light ---
  for (int i = 0; i < NUM_PIXELS; i++) {
    float phase = fmod(i - offset + NUM_PIXELS * SPACING, (float)SPACING);
    float dist = min(phase, (float)SPACING - phase);

    int brightness;
    if      (dist < 0.5) brightness = 60;
    else if (dist < 1.5) brightness = 30;
    else if (dist < 2.5) brightness = 15;
    else                  brightness = 7;

    strip.setPixelColor(i, strip.Color(brightness, 0, 0));
  }
  strip.show();

  // --- Strip 2: rainbow / solid red pattern ---
  uint32_t rainbowColor = strip2.gamma32(strip2.ColorHSV(rainbowHue, 255, 200));

  // pixels 0-2: rainbow
  strip2.setPixelColor(0, rainbowColor);
  strip2.setPixelColor(1, rainbowColor);
  strip2.setPixelColor(2, rainbowColor);
  // pixels 3-4: solid red
  strip2.setPixelColor(3, strip2.Color(200, 0, 0));
  strip2.setPixelColor(4, strip2.Color(200, 0, 0));
  // pixel 5: rainbow
  strip2.setPixelColor(5, rainbowColor);

  strip2.show();

  rainbowHue += 300;  // ~3.5 second full color cycle

  delay(10);

  offset += SPEED;
  if (offset >= SPACING) offset -= SPACING;
}
