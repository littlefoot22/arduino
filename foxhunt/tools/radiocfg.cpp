/// Finds the largest buffer RadioLoadConfig() survives on this firmware.
///
/// Passing a 56-byte register list to that call hard-reset the board: the
/// heartbeat stopped, USB dropped, and the host logged "Device not configured".
/// step10 passed six bytes to the same call and was fine. That difference
/// points at a fixed-size buffer in firmware rather than at the register
/// contents, and RadioLoadConfig() is documented @todo with no vendor example,
/// so both its expected size and its layout are guesses.
///
/// One size per binary, deliberately. A single escalating binary would lose its
/// own evidence the moment the device reset - screen cleared, LEDs cleared - so
/// instead each build tries exactly one size and says so on screen first.
///
///   shows "cfg N ok" and keeps pulsing   N bytes is safe
///   board resets, USB drops              N bytes is too many
///
/// Build via the radiocfg2 .. radiocfg56 CMake targets.

#include <fwwasm.h>

#ifndef CFG_BYTES
#error "CFG_BYTES must be defined"
#endif

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

namespace {

/// Valid CC1101 address/value pairs, so a firmware that does interpret the
/// contents is given something sane rather than noise. FREQ2/FREQ1/FREQ0 for
/// 446.025 MHz, then harmless repeats of the same three registers.
unsigned char g_config[CFG_BYTES];

void fill_config() {
    const unsigned char pattern[6] = {0x0D, 0x11, 0x0E, 0x2B, 0x0F, 0x62};
    for (int i = 0; i < CFG_BYTES; ++i) {
        g_config[i] = pattern[i % 6];
    }
}

}  // namespace

int main() {
    setBoardLED(0, 0, 255, 0, 60000, ledsimplevalue);

    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);
    // Announce the size BEFORE attempting it. If the board resets, the last
    // thing seen is the size that did it.
    addControlText(1, 0, 20, 40, 1, 34, 255, 255, 255, "cfg " STRINGIFY(CFG_BYTES) " trying");
    showPanel(1);

    // Give the display a moment to actually paint before risking the reset.
    waitms(600);

    fill_config();

    RadioSetIdle(1);
    const int rc = RadioLoadConfig(1, g_config, CFG_BYTES);
    RadioSetRx(1);

    // Survived.
    setBoardLED(1, 0, 0, 255, 60000, ledsimplevalue);
    addControlText(1, 1, 20, 100, 1, 34, 0, 255, 0, "cfg " STRINGIFY(CFG_BYTES) " ok");

    addControlNumber(1, 2, 1, 20, 150, 200, 3, 0, 255, 255, 255, 0, 0, 0, 0);
    setControlValue(1, 2, rc);

    // Show live RSSI too: if the config took, this should differ from what a
    // build with a different frequency reports.
    addControlNumber(1, 3, 1, 20, 190, 200, 3, 0, 255, 255, 0, 0, 0, 0, 0);

    while (true) {
        setControlValue(1, 3, RadioGetRSSI(1));
        setBoardLED(6, 255, 0, 0, 400, ledpulsefade);
        waitms(500);
    }

    return 0;
}
