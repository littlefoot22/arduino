#include <Adafruit_NeoPixel.h>

#define PIN        12
#define NUM_PIXELS 10
#define SPACING    6

#define PIN2        9
#define NUM_PIXELS2 6

#define PIN3        6
#define NUM_PIXELS3 16
#define CITY_PIXELS 13

#define PIN4        10
#define NUM_PIXELS4 7

#define BTN_PATTERN 7
#define BTN_BRIGHT  8
#define LONG_PRESS_MS 700

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_PIXELS2, PIN2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3(NUM_PIXELS3, PIN3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip4(NUM_PIXELS4, PIN4, NEO_GRB + NEO_KHZ800);

// --- Brightness ---
const uint8_t BRIGHT_LEVELS[] = {20, 60, 130, 255};
const int NUM_BRIGHT_LEVELS = 4;
int brightIndex = 2;

// --- Pattern ---
int patternIndex = 0;
#define NUM_PATTERNS 3

// --- Button state ---
bool btn7Last = HIGH, btn8Last = HIGH;
unsigned long btn7PressTime = 0, btn8PressTime = 0;

// --- Strip 1 ---
float offset = 0.0;
const float SPEED = 0.05;

// --- Strip 2 pattern state ---
uint16_t rainbowHue = 0;
uint16_t tokyoGradOffset = 0;

// Strip 2 pattern 2: star twinkle
uint16_t starPhase[NUM_PIXELS2];
uint16_t starHue[NUM_PIXELS2];
bool starIsColor[NUM_PIXELS2];
uint8_t starSpeed[NUM_PIXELS2];

// --- Strip 3 ---
uint32_t cityColors[CITY_PIXELS];
int cityTimer = 0;

// --- Strip 4 ---
uint16_t neonHue[5] = {0, 16384, 32768, 49152, 8192};
int twinkleTimer = 0;

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

void initStars() {
  for (int i = 0; i < NUM_PIXELS2; i++) {
    starPhase[i]   = random(180);
    starSpeed[i]   = random(4, 10);
    starIsColor[i] = (random(5) == 0);
    starHue[i]     = random(65536);
  }
}

void setAllBrightness() {
  uint8_t b = BRIGHT_LEVELS[brightIndex];
  strip.setBrightness(b);
  strip2.setBrightness(b);
  strip3.setBrightness(b);
  strip4.setBrightness(b);
}

void handleButtons() {
  bool btn7 = digitalRead(BTN_PATTERN);
  bool btn8 = digitalRead(BTN_BRIGHT);
  unsigned long now = millis();

  // Both held: reset to default brightness and pattern 0
  if (btn7 == LOW && btn8 == LOW) {
    brightIndex = 2;
    patternIndex = 0;
    setAllBrightness();
    delay(500);
    return;
  }

  // Button 7 press
  if (btn7 == LOW && btn7Last == HIGH) btn7PressTime = now;
  // Button 7 release: cycle pattern (short press only)
  if (btn7 == HIGH && btn7Last == LOW) {
    patternIndex = (patternIndex + 1) % NUM_PATTERNS;
  }

  // Button 8 press
  if (btn8 == LOW && btn8Last == HIGH) btn8PressTime = now;
  // Button 8 release: short = brightness up, long = brightness down
  if (btn8 == HIGH && btn8Last == LOW) {
    if (now - btn8PressTime < LONG_PRESS_MS) {
      brightIndex = min(brightIndex + 1, NUM_BRIGHT_LEVELS - 1);
    } else {
      brightIndex = max(brightIndex - 1, 0);
    }
    setAllBrightness();
  }

  btn7Last = btn7;
  btn8Last = btn8;
}

// Add new strip2 patterns here as new case blocks
void drawStrip2() {
  switch (patternIndex) {
    case 0: {
      // Rainbow wash + solid red center
      uint32_t c = strip2.gamma32(strip2.ColorHSV(rainbowHue, 255, 200));
      strip2.setPixelColor(0, c);
      strip2.setPixelColor(1, c);
      strip2.setPixelColor(2, c);
      strip2.setPixelColor(3, strip2.Color(200, 0, 0));
      strip2.setPixelColor(4, strip2.Color(200, 0, 0));
      strip2.setPixelColor(5, c);
      break;
    }
    case 1: {
      // Tokyo neon gradient drifting downward (cyan -> purple -> pink range)
      for (int i = 0; i < NUM_PIXELS2; i++) {
        uint16_t hue = 32768 + ((tokyoGradOffset + (uint16_t)(i * 5461)) % 32768);
        strip2.setPixelColor(i, strip2.gamma32(strip2.ColorHSV(hue, 255, 180)));
      }
      tokyoGradOffset += 60;
      break;
    }
    case 2: {
      // Stars: mostly white twinkles, 20% random color, each pixel independent
      const uint16_t STAR_PERIOD = 180;
      for (int i = 0; i < NUM_PIXELS2; i++) {
        starPhase[i] += starSpeed[i];
        if (starPhase[i] >= STAR_PERIOD) {
          starPhase[i]   = 0;
          starIsColor[i] = (random(5) == 0);
          starHue[i]     = random(65536);
          starSpeed[i]   = random(4, 10);
        }
        uint16_t p = starPhase[i];
        uint8_t bright = (p < STAR_PERIOD / 2)
          ? (uint8_t)((p * 255) / (STAR_PERIOD / 2))
          : (uint8_t)(((STAR_PERIOD - p) * 255) / (STAR_PERIOD / 2));
        uint32_t c = starIsColor[i]
          ? strip2.gamma32(strip2.ColorHSV(starHue[i], 220, bright))
          : strip2.Color(bright, bright, bright);
        strip2.setPixelColor(i, c);
      }
      break;
    }
    // case 3: add pattern 4 here
  }
  strip2.show();
}

void setup() {
  strip.begin();  strip.show();
  strip2.begin(); strip2.show();
  strip3.begin(); strip3.show();
  strip4.begin(); strip4.show();

  pinMode(BTN_PATTERN, INPUT_PULLUP);
  pinMode(BTN_BRIGHT,  INPUT_PULLUP);

  randomSeed(analogRead(A0));
  for (int i = 0; i < CITY_PIXELS; i++) randomizeCityPixel(i);
  initStars();

  setAllBrightness();
}

void loop() {
  handleButtons();

  // --- Strip 1: traveling red light ---
  for (int i = 0; i < NUM_PIXELS; i++) {
    float phase = fmod(i - offset + NUM_PIXELS * SPACING, (float)SPACING);
    float dist = min(phase, (float)SPACING - phase);

    int brightness;
    if      (dist < 0.5) brightness = 15;
    else if (dist < 1.5) brightness = 8;
    else if (dist < 2.5) brightness = 4;
    else                  brightness = 1;

    strip.setPixelColor(i, strip.Color(brightness, 0, 0));
  }
  strip.show();

  // --- Strip 2: pattern ---
  drawStrip2();

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

  // --- Strip 4: Tokyo neon (0-4) + dim blue twinkle (5-6) ---
  neonHue[0] += 150;
  neonHue[1] += 210;
  neonHue[2] += 175;
  neonHue[3] += 240;
  neonHue[4] += 195;
  for (int i = 0; i < 5; i++) {
    strip4.setPixelColor(i, strip4.gamma32(strip4.ColorHSV(neonHue[i], 255, 20)));
  }
  twinkleTimer++;
  strip4.setPixelColor(5, strip4.Color(0, 0, twinkleBrightness(twinkleTimer)));
  strip4.setPixelColor(6, strip4.Color(0, 0, twinkleBrightness(twinkleTimer + 200)));
  strip4.show();

  rainbowHue += 300;

  delay(10);

  offset += SPEED;
  if (offset >= SPACING) offset -= SPACING;
}
