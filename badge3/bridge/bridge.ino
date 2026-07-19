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
#define NUM_PIXELS4 2

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
int brightIndex[NUM_STRIPS] = {2, 2, 2, 2};

// --- Pattern ---
#define NUM_STRIPS 4
int patternIndex[NUM_STRIPS] = {0, 0, 0, 0};
const int NUM_STRIP_PATTERNS[NUM_STRIPS] = {4, 7, 5, 7};
int selectedStrip = 1;

// --- Button state ---
bool btn7Last = HIGH, btn8Last = HIGH;
unsigned long btn7PressTime = 0, btn8PressTime = 0;
bool btn7WaitingDoubleTap = false;
unsigned long btn7LastReleaseTime = 0;
bool btn8WaitingDoubleTap = false;
unsigned long btn8LastReleaseTime = 0;
#define DOUBLE_TAP_MS 350

// --- Strip 1 ---
float offset = 0.0;
const float SPEED = 0.05;
uint16_t bridgeRainbowOffset = 0;
float bridgeCarA = 0.0;
float bridgeCarB = 5.0;
float bridgeFogPos = 0.0;

// --- Strip 2 pattern state ---
uint16_t rainbowHue = 0;
uint16_t tokyoGradOffset = 0;
uint16_t warmNeonOffset = 0;
uint16_t moodyHue = 0;

// Strip 2 pattern 2: star twinkle
uint16_t starPhase[NUM_PIXELS2];
uint16_t starHue[NUM_PIXELS2];
bool starIsColor[NUM_PIXELS2];
uint8_t starSpeed[NUM_PIXELS2];

// --- Strip 3 ---
uint32_t cityColors[CITY_PIXELS];
int cityTimer = 0;
uint16_t sunsetOffset = 0;
uint32_t tsColors[NUM_PIXELS3];
uint8_t  tsTimers[NUM_PIXELS3];
uint8_t  blackoutBright[NUM_PIXELS3];
int8_t   blackoutDir[NUM_PIXELS3];
uint16_t blackoutWait[NUM_PIXELS3];
uint8_t  rainLightning[NUM_PIXELS3];
uint16_t rainCountdown = 300;
bool     rainLightningActive = false;

// --- Strip 4 ---
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

void flashSelectedStrip() {
  Adafruit_NeoPixel* s;
  int n;
  switch (selectedStrip) {
    case 0: s = &strip;  n = NUM_PIXELS;  break;
    case 1: s = &strip2; n = NUM_PIXELS2; break;
    case 2: s = &strip3; n = NUM_PIXELS3; break;
    default: s = &strip4; n = NUM_PIXELS4; break;
  }
  for (int i = 0; i < n; i++) s->setPixelColor(i, s->Color(200, 200, 200));
  s->show();
  delay(200);
  for (int i = 0; i < n; i++) s->setPixelColor(i, 0);
  s->show();
  delay(100);
}

void setStripBrightness(int s) {
  uint8_t b = BRIGHT_LEVELS[brightIndex[s]];
  switch (s) {
    case 0: strip.setBrightness(b);  break;
    case 1: strip2.setBrightness(b); break;
    case 2: strip3.setBrightness(b); break;
    case 3: strip4.setBrightness(b); break;
  }
}

void setAllBrightness() {
  for (int i = 0; i < NUM_STRIPS; i++) setStripBrightness(i);
}

void handleButtons() {
  bool btn7 = digitalRead(BTN_PATTERN);
  bool btn8 = digitalRead(BTN_BRIGHT);
  unsigned long now = millis();

  // Both held: reset everything
  if (btn7 == LOW && btn8 == LOW) {
    for (int i = 0; i < NUM_STRIPS; i++) { patternIndex[i] = 0; brightIndex[i] = 2; }
    selectedStrip = 1;
    btn7WaitingDoubleTap = false;
    btn8WaitingDoubleTap = false;
    setAllBrightness();
    delay(500);
    return;
  }

  // Button 7 press
  if (btn7 == LOW && btn7Last == HIGH) btn7PressTime = now;

  // Button 7 release (short press only)
  if (btn7 == HIGH && btn7Last == LOW && (now - btn7PressTime < LONG_PRESS_MS)) {
    if (btn7WaitingDoubleTap && (now - btn7LastReleaseTime < DOUBLE_TAP_MS)) {
      // Double tap: change selected strip + flash indicator
      btn7WaitingDoubleTap = false;
      selectedStrip = (selectedStrip + 1) % NUM_STRIPS;
      flashSelectedStrip();
    } else {
      // First tap: open double tap window
      btn7WaitingDoubleTap = true;
      btn7LastReleaseTime = now;
    }
  }

  // Double tap window expired: commit single tap (cycle pattern on selected strip)
  if (btn7WaitingDoubleTap && (now - btn7LastReleaseTime >= DOUBLE_TAP_MS)) {
    btn7WaitingDoubleTap = false;
    patternIndex[selectedStrip] = (patternIndex[selectedStrip] + 1) % NUM_STRIP_PATTERNS[selectedStrip];
  }

  // Button 8 press
  if (btn8 == LOW && btn8Last == HIGH) btn8PressTime = now;

  // Button 8 release (short press only)
  if (btn8 == HIGH && btn8Last == LOW && (now - btn8PressTime < LONG_PRESS_MS)) {
    if (btn8WaitingDoubleTap && (now - btn8LastReleaseTime < DOUBLE_TAP_MS)) {
      // Double tap: brightness down on selected strip
      btn8WaitingDoubleTap = false;
      brightIndex[selectedStrip] = max(brightIndex[selectedStrip] - 1, 0);
      setStripBrightness(selectedStrip);
    } else {
      // First tap: open double tap window
      btn8WaitingDoubleTap = true;
      btn8LastReleaseTime = now;
    }
  }

  // Double tap window expired: commit single tap (brightness up on selected strip)
  if (btn8WaitingDoubleTap && (now - btn8LastReleaseTime >= DOUBLE_TAP_MS)) {
    btn8WaitingDoubleTap = false;
    brightIndex[selectedStrip] = min(brightIndex[selectedStrip] + 1, NUM_BRIGHT_LEVELS - 1);
    setStripBrightness(selectedStrip);
  }

  btn7Last = btn7;
  btn8Last = btn8;
}

void drawStrip1() {
  switch (patternIndex[0]) {
    case 0: {
      // Traveling red light (original)
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
      offset += SPEED;
      if (offset >= SPACING) offset -= SPACING;
      break;
    }
    case 1: {
      // Rainbow gradient sliding left to right
      bridgeRainbowOffset += 200;
      for (int i = 0; i < NUM_PIXELS; i++) {
        uint16_t hue = bridgeRainbowOffset + (uint16_t)(i * (65536 / NUM_PIXELS));
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue, 255, 180)));
      }
      break;
    }
    case 2: {
      // Headlights & taillights: two cars going opposite directions
      bridgeCarA += 0.04;
      if (bridgeCarA >= NUM_PIXELS) bridgeCarA -= NUM_PIXELS;
      bridgeCarB -= 0.03;
      if (bridgeCarB < 0) bridgeCarB += NUM_PIXELS;
      for (int i = 0; i < NUM_PIXELS; i++) {
        float distA = fabs(i - bridgeCarA);
        if (distA > NUM_PIXELS / 2.0) distA = NUM_PIXELS - distA;
        float distB = fabs(i - bridgeCarB);
        if (distB > NUM_PIXELS / 2.0) distB = NUM_PIXELS - distB;
        uint8_t wa = (distA < 0.5) ? 60 : (distA < 1.5) ? 30 : (distA < 2.5) ? 10 : 0;
        uint8_t rb = (distB < 0.5) ? 50 : (distB < 1.5) ? 25 : (distB < 2.5) ? 8 : 0;
        strip.setPixelColor(i, strip.Color(
          (uint8_t)min(255, (int)wa + rb),
          (uint8_t)(wa * 9 / 10),
          (uint8_t)(wa * 8 / 10)
        ));
      }
      break;
    }
    case 3: {
      // Foggy night: dim cool-white base with single drifting bright spot
      bridgeFogPos += 0.02;
      if (bridgeFogPos >= NUM_PIXELS) bridgeFogPos -= NUM_PIXELS;
      for (int i = 0; i < NUM_PIXELS; i++) {
        float dist = fabs(i - bridgeFogPos);
        if (dist > NUM_PIXELS / 2.0) dist = NUM_PIXELS - dist;
        uint8_t spot = (dist < 0.5) ? 35 : (dist < 1.5) ? 20 : (dist < 2.5) ? 10 : 0;
        uint8_t b = 6 + spot;
        strip.setPixelColor(i, strip.Color((b * 4) / 5, (b * 4) / 5, b));
      }
      break;
    }
  }
  strip.show();
}

uint32_t sunsetColor(uint8_t pos) {
  if (pos < 85) {
    uint8_t t = pos;
    return strip3.Color(220, 100 - t, (t * 3) / 2);
  } else if (pos < 170) {
    uint8_t t = pos - 85;
    return strip3.Color(220 - t, 0, 127 + t / 2);
  } else {
    uint8_t t = pos - 170;
    return strip3.Color(135 - t, 0, 170 - t / 2);
  }
}

void initStrip3Patterns() {
  for (int i = 0; i < NUM_PIXELS3; i++) {
    tsColors[i]       = strip3.gamma32(strip3.ColorHSV(random(65536), 255, random(150, 255)));
    tsTimers[i]       = random(3, 20);
    blackoutBright[i] = 0;
    blackoutDir[i]    = 0;
    blackoutWait[i]   = random(20, 150);
    rainLightning[i]  = 0;
  }
}

void drawStrip3() {
  switch (patternIndex[2]) {
    case 0: {
      // NYC skyline + One WTC rainbow top
      cityTimer++;
      if (cityTimer >= 60) {
        cityTimer = 0;
        randomizeCityPixel(random(CITY_PIXELS));
      }
      for (int i = 0; i < CITY_PIXELS; i++) strip3.setPixelColor(i, cityColors[i]);
      uint32_t wtcColor = strip3.gamma32(strip3.ColorHSV(rainbowHue, 255, 210));
      strip3.setPixelColor(13, wtcColor);
      strip3.setPixelColor(14, wtcColor);
      strip3.setPixelColor(15, wtcColor);
      break;
    }
    case 1: {
      // Sunset/dusk: amber -> rose -> purple gradient slowly drifting
      sunsetOffset++;
      for (int i = 0; i < NUM_PIXELS3; i++) {
        uint8_t pos = (uint8_t)(sunsetOffset / 4) + i * 10;
        strip3.setPixelColor(i, sunsetColor(pos));
      }
      break;
    }
    case 2: {
      // Times Square: rapid random color pops per pixel
      for (int i = 0; i < NUM_PIXELS3; i++) {
        if (tsTimers[i] == 0) {
          tsColors[i] = strip3.gamma32(strip3.ColorHSV(random(65536), 255, random(150, 255)));
          tsTimers[i] = random(3, 20);
        } else {
          tsTimers[i]--;
        }
        strip3.setPixelColor(i, tsColors[i]);
      }
      break;
    }
    case 3: {
      // Blackout: most buildings dark, random windows fade in/out
      for (int i = 0; i < NUM_PIXELS3; i++) {
        if (blackoutDir[i] == 0) {
          if (blackoutWait[i] > 0) {
            blackoutWait[i]--;
          } else {
            blackoutDir[i] = (blackoutBright[i] == 0) ? 1 : -1;
          }
        } else if (blackoutDir[i] == 1) {
          blackoutBright[i] += 2;
          if (blackoutBright[i] >= 40) {
            blackoutBright[i] = 40;
            blackoutDir[i] = 0;
            blackoutWait[i] = random(100, 400);
          }
        } else {
          if (blackoutBright[i] <= 2) {
            blackoutBright[i] = 0;
            blackoutDir[i] = 0;
            blackoutWait[i] = random(50, 300);
          } else {
            blackoutBright[i] -= 2;
          }
        }
        uint8_t b = blackoutBright[i];
        strip3.setPixelColor(i, strip3.Color(b, (b * 2) / 3, b / 5));
      }
      break;
    }
    case 4: {
      // Rainy night: dim blue-grey base with random lightning flashes
      if (!rainLightningActive) {
        if (rainCountdown > 0) {
          rainCountdown--;
        } else {
          rainLightningActive = true;
          rainCountdown = random(150, 600);
          int start = random(NUM_PIXELS3 - 3);
          int len = random(1, 4);
          for (int i = 0; i < NUM_PIXELS3; i++) rainLightning[i] = 0;
          for (int i = start; i < start + len && i < NUM_PIXELS3; i++) {
            rainLightning[i] = 200;
          }
        }
      }
      if (rainLightningActive) {
        bool any = false;
        for (int i = 0; i < NUM_PIXELS3; i++) {
          if (rainLightning[i] > 0) {
            rainLightning[i] = (rainLightning[i] > 25) ? rainLightning[i] - 25 : 0;
            any = true;
          }
        }
        if (!any) rainLightningActive = false;
      }
      for (int i = 0; i < NUM_PIXELS3; i++) {
        uint8_t l = rainLightning[i];
        strip3.setPixelColor(i, strip3.Color(
          (uint8_t)min(255, 3 + (int)l),
          (uint8_t)min(255, 5 + (int)l),
          (uint8_t)min(255, 10 + (int)l)
        ));
      }
      break;
    }
  }
  strip3.show();
}

// Add new strip2 patterns here as new case blocks
void drawStrip2() {
  switch (patternIndex[1]) {
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
    case 5: {
      // Warm neon gradient: hot pink -> red -> orange -> yellow, drifting
      for (int i = 0; i < NUM_PIXELS2; i++) {
        uint16_t hue = (uint16_t)(58000 + warmNeonOffset + (uint16_t)(i * 3640));
        strip2.setPixelColor(i, strip2.gamma32(strip2.ColorHSV(hue, 255, 180)));
      }
      warmNeonOffset += 60;
      break;
    }
    case 3: {
      // Blue center + moody cool surround (deep blues/purples/teals, slow shift)
      moodyHue += 30;
      uint16_t coolHue = 21845 + (moodyHue % 27307);
      uint32_t c = strip2.gamma32(strip2.ColorHSV(coolHue, 220, 120));
      strip2.setPixelColor(0, c);
      strip2.setPixelColor(1, c);
      strip2.setPixelColor(2, c);
      strip2.setPixelColor(3, strip2.Color(0, 0, 180));
      strip2.setPixelColor(4, strip2.Color(0, 0, 180));
      strip2.setPixelColor(5, c);
      break;
    }
    case 4: {
      // Colorful star twinkle: full rainbow palette, every pixel always a saturated color
      const uint16_t STAR_PERIOD = 180;
      for (int i = 0; i < NUM_PIXELS2; i++) {
        starPhase[i] += starSpeed[i];
        if (starPhase[i] >= STAR_PERIOD) {
          starPhase[i] = 0;
          starHue[i]   = random(65536);
          starSpeed[i] = random(4, 10);
        }
        uint16_t p = starPhase[i];
        uint8_t bright = (p < STAR_PERIOD / 2)
          ? (uint8_t)((p * 255) / (STAR_PERIOD / 2))
          : (uint8_t)(((STAR_PERIOD - p) * 255) / (STAR_PERIOD / 2));
        strip2.setPixelColor(i, strip2.gamma32(strip2.ColorHSV(starHue[i], 255, bright)));
      }
      break;
    }
    case 6: {
      // Full rainbow gradient across all pixels, slowly cycling
      for (int i = 0; i < NUM_PIXELS2; i++) {
        uint16_t hue = rainbowHue + (uint16_t)(i * (65536 / NUM_PIXELS2));
        strip2.setPixelColor(i, strip2.gamma32(strip2.ColorHSV(hue, 255, 200)));
      }
      break;
    }
  }
  strip2.show();
}

void drawStrip4() {
  twinkleTimer++;
  int b0 = twinkleBrightness(twinkleTimer);
  int b1 = twinkleBrightness(twinkleTimer + 200);
  switch (patternIndex[3]) {
    case 0: // Blue
      strip4.setPixelColor(0, strip4.Color(0, 0, b0));
      strip4.setPixelColor(1, strip4.Color(0, 0, b1));
      break;
    case 1: // Red
      strip4.setPixelColor(0, strip4.Color(b0, 0, 0));
      strip4.setPixelColor(1, strip4.Color(b1, 0, 0));
      break;
    case 2: // Green
      strip4.setPixelColor(0, strip4.Color(0, b0, 0));
      strip4.setPixelColor(1, strip4.Color(0, b1, 0));
      break;
    case 3: // White
      strip4.setPixelColor(0, strip4.Color(b0, b0, b0));
      strip4.setPixelColor(1, strip4.Color(b1, b1, b1));
      break;
    case 4: // Purple
      strip4.setPixelColor(0, strip4.Color(b0 / 2, 0, b0));
      strip4.setPixelColor(1, strip4.Color(b1 / 2, 0, b1));
      break;
    case 5: // Rainbow twinkle: two pixels on opposite sides of color wheel
      strip4.setPixelColor(0, strip4.gamma32(strip4.ColorHSV(rainbowHue, 255, b0)));
      strip4.setPixelColor(1, strip4.gamma32(strip4.ColorHSV(rainbowHue + 32768, 255, b1)));
      break;
    case 6: // Full rainbow gradient, solid (no twinkle)
      strip4.setPixelColor(0, strip4.gamma32(strip4.ColorHSV(rainbowHue, 255, 200)));
      strip4.setPixelColor(1, strip4.gamma32(strip4.ColorHSV(rainbowHue + 32768, 255, 200)));
      break;
  }
  strip4.show();
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
  initStrip3Patterns();

  setAllBrightness();
}

void loop() {
  handleButtons();

  // --- Strip 1: bridge pattern ---
  drawStrip1();

  // --- Strip 2: pattern ---
  drawStrip2();

  // --- Strip 3: city pattern ---
  drawStrip3();

  // --- Strip 4: color twinkle ---
  drawStrip4();

  rainbowHue += 300;

  delay(10);
}
