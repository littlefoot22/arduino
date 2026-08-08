/// Smallest possible FreeWili app: one panel, one string, nothing else.
///
/// This exists to answer a single question - can a WASM app on this device put
/// text on the screen at all? Everything that could possibly be omitted has
/// been. The call sequence and argument values are copied from the vendor radio
/// example, which is known to work on this hardware.
///
/// Built with a stricter feature set than the rest of the project: plain
/// -mcpu=mvp with no bulk-memory, so it is the most conservative WebAssembly
/// this toolchain can emit. If the interpreter refuses even this, the problem
/// is not the module.
///
/// LED 0 turns green before anything is drawn, so proof-of-life does not depend
/// on the display working.

#include <fwwasm.h>

int main() {
    // Proof of life first, independent of the screen.
    setBoardLED(0, 0, 255, 0, 60000, ledsimplevalue);
    playSoundFromFrequencyAndDuration(880.0F, 0.15F, 0.2F, WAVETYPE_SINE);

    // Black background, menu bar on - exactly as the vendor example does it.
    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);

    // fontType 1, fontSize 64: the literal values from the vendor example.
    addControlText(1, 1, 20, 90, 1, 64, 255, 255, 255, "HELLO");

    showPanel(1);

    // LED 1 confirms every display call returned.
    setBoardLED(1, 0, 0, 255, 60000, ledsimplevalue);

    while (true) {
        waitms(1000);
    }

    return 0;
}
