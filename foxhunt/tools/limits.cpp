/// Probes how many panels and controls this firmware will actually accept.
///
/// Every host call the app needs is known to work (tools/step.cpp reaches its
/// top rung), yet the full app still draws nothing. The remaining differences
/// between it and a working step build are structural: how many panels exist,
/// how many controls each holds, and how high the indices go. The vendor
/// examples never exceed roughly ten controls on a panel, so a fixed-size array
/// in the firmware is the obvious suspect - and an overrun would explain a
/// screen that simply never appears.
///
///   LIMIT  tests
///     1    one panel, 12 controls at indices 0-11   (what the app now uses)
///     2    one panel, 20 controls at indices 0-19
///     3    one panel, one control at sparse index 36 (what the app used before)
///     4    four panels, few controls each            (the app's panel count)
///
/// Each build draws its label first, so a blank screen means the structure
/// under test broke it rather than the drawing itself.

#include <fwwasm.h>

#ifndef LIMIT
#error "LIMIT must be defined (1-4)"
#endif

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

namespace {

constexpr const char* kLabel = "LIMIT " STRINGIFY(LIMIT);

void led(int index, int r, int g, int b) { setBoardLED(index, r, g, b, 60000, ledsimplevalue); }

}  // namespace

int main() {
    led(0, 0, 255, 0);
    playSoundFromFrequencyAndDuration(880.0F, 0.15F, 0.2F, WAVETYPE_SINE);

    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);
    addControlText(1, 0, 20, 20, 1, 32, 255, 255, 255, kLabel);

#if LIMIT == 1 || LIMIT == 2
#if LIMIT == 1
    constexpr int kCount = 12;
#else
    constexpr int kCount = 20;
#endif
    // Fill indices 1..kCount-1 with small text markers in a grid.
    for (int i = 1; i < kCount; ++i) {
        const int x = 20 + ((i % 6) * 48);
        const int y = 70 + ((i / 6) * 40);
        addControlText(1, i, x, y, 0, 20, 0, 255, 0, "#");
    }
#elif LIMIT == 3
    // A single control at the high sparse index the app originally used.
    addControlText(1, 36, 20, 100, 1, 28, 255, 255, 0, "index 36");
#elif LIMIT == 4
    // Four panels, as the app has, each with a couple of controls.
    for (int p = 2; p <= 4; ++p) {
        addPanel(p, 1, 0, 0, 0, 0, 0, 0, 1);
        addControlText(p, 0, 20, 20, 1, 32, 255, 255, 255, "other panel");
        addControlText(p, 1, 20, 70, 1, 20, 200, 200, 200, "second control");
    }
#endif

    showPanel(1);
    led(1, 0, 0, 255);

    while (true) {
        setBoardLED(6, 255, 0, 0, 400, ledpulsefade);
        waitms(500);
    }

    return 0;
}
