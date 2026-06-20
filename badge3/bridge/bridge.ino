#include <Adafruit_NeoPixel.h>

#define PIN        8
#define NUM_PIXELS 10
#define SPACING    6

#define PIN2        9
#define NUM_PIXELS2 6

#define PIN3        6
#define NUM_PIXELS3 16
#define CITY_PIXELS 13

#define PIN4        10
#define NUM_PIXELS4 6

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_PIXELS2, PIN2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(NUM_PIXELS3, PIN3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip4(NUM_PIXELS4, PIN4, NEO_GRB + NEO_KHZ800);

float offset = 0.0;
const float SPEED = 0.05;

uint16_t rainbowHue = 0;

uint32_t cityColors[CITY_PIXELS];
int cityTimer = 0;

// each neon pixel starts at a different point on the color wheel
uint16_t neonHue[4] = {0, 16384, 32768, 49152};
int twinkleTimer = 0;

// smooth fade: dim base, brief pulse up every ~4 seconds
int twinkleBrightness(int t) {
  const int PERIOD = 400;
  const int FADE   = 40;
  int phase = ((t % PERIOD) + PERIOD) % PERIOD;
  if (phase < PERIOD - FADE * 2) return 6;
  phase -= (PERIOD - FADE * 2);
  if (phase < FADE) return 6 + (phase * 44) / FADE;
  return 50 - ((phase - FADE) * 44) / FADE;
}

void randomizeCityPixel(int i) {
  int r = random(100);
  if (r < 55) {
    cityColors[i] = strip3.Color(random(10, 35), random(70, 120), random(130, 190));
  } else if (r < 75) {
    cityColors[i] = strip3.Color(random(160, 220), random(90, 140), random(0, 15));
  } else if (r < 90) {
    int w = random(100, 170);
    cityColors[i] = strip3.Color(w - 10, w - 5, w);
  } else {
    cityColors[i] = strip3.Color(0, random(5, 18), random(20, 45));
  }
}

void setup() {
  strip.begin();  strip.show();
  strip2.begin(); strip2.show();
  strip3.begin(); strip3.show();
  strip4.begin(); strip4.show();

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

  // --- Strip 3: NYC skyline + One WTC rainbow top ---
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

  // --- Strip 4: Tokyo neon (0-3) + dim blue twinkle (4-5) ---
  neonHue[0] += 150;
  neonHue[1] += 210;
  neonHue[2] += 175;
  neonHue[3] += 240;
  for (int i = 0; i < 4; i++) {
    strip4.setPixelColor(i, strip4.gamma32(strip4.ColorHSV(neonHue[i], 255, 190)));
  }
  twinkleTimer++;
  // offset pixel 5 by half the period so they twinkle at different times
  strip4.setPixelColor(4, strip4.Color(0, 0, twinkleBrightness(twinkleTimer)));
  strip4.setPixelColor(5, strip4.Color(0, 0, twinkleBrightness(twinkleTimer + 200)));
  strip4.show();

  rainbowHue += 300;

  delay(10);

  offset += SPEED;
  if (offset >= SPACING) offset -= SPACING;
}
