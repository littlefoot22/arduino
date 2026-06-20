#include <Adafruit_NeoPixel.h>

#define PIN        8
#define NUM_PIXELS 10
#define SPACING    6

#define PIN2        9
#define NUM_PIXELS2 6

#define PIN3        6
#define NUM_PIXELS3 16
#define CITY_PIXELS 13

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_PIXELS2, PIN2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(NUM_PIXELS3, PIN3, NEO_GRB + NEO_KHZ800);

float offset = 0.0;
const float SPEED = 0.05;

uint16_t rainbowHue = 0;

uint32_t cityColors[CITY_PIXELS];
int cityTimer = 0;

void randomizeCityPixel(int i) {
  int r = random(100);
  if (r < 55) {
    // light blue — glass/steel building ambient glow
    cityColors[i] = strip3.Color(random(10, 35), random(70, 120), random(130, 190));
  } else if (r < 75) {
    // warm yellow — incandescent office window
    cityColors[i] = strip3.Color(random(160, 220), random(90, 140), random(0, 15));
  } else if (r < 90) {
    // cool white — bright fluorescent window
    int w = random(100, 170);
    cityColors[i] = strip3.Color(w - 10, w - 5, w);
  } else {
    // dark — unlit building face
    cityColors[i] = strip3.Color(0, random(5, 18), random(20, 45));
  }
}

void setup() {
  strip.begin();
  strip.show();
  strip2.begin();
  strip2.show();
  strip3.begin();
  strip3.show();

  randomSeed(analogRead(A0));
  for (int i = 0; i < CITY_PIXELS; i++) {
    randomizeCityPixel(i);
  }
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
  strip2.setPixelColor(0, rainbowColor);
  strip2.setPixelColor(1, rainbowColor);
  strip2.setPixelColor(2, rainbowColor);
  strip2.setPixelColor(3, strip2.Color(200, 0, 0));
  strip2.setPixelColor(4, strip2.Color(200, 0, 0));
  strip2.setPixelColor(5, rainbowColor);
  strip2.show();

  // --- Strip 3: NYC skyline (0-12) + One WTC rainbow top (13-15) ---
  cityTimer++;
  if (cityTimer >= 60) {
    cityTimer = 0;
    randomizeCityPixel(random(CITY_PIXELS));
  }
  for (int i = 0; i < CITY_PIXELS; i++) {
    strip3.setPixelColor(i, cityColors[i]);
  }
  uint32_t wtcColor = strip3.gamma32(strip3.ColorHSV(rainbowHue, 255, 210));
  strip3.setPixelColor(13, wtcColor);
  strip3.setPixelColor(14, wtcColor);
  strip3.setPixelColor(15, wtcColor);
  strip3.show();

  rainbowHue += 300;

  delay(10);

  offset += SPEED;
  if (offset >= SPACING) offset -= SPACING;
}
