/// Staged startup probe: reports how far an app got using only the board LEDs.
///
/// LEDs and sound are driven independently of the display, which is what makes
/// them useful when the display is the thing under suspicion.
///
///   LED 0 white    main() entered - the module loaded and is executing
///   LED 1 blue     addPanel() returned
///   LED 2 green    addControlText() returned
///   LED 3 yellow   showPanel() returned
///   LED 4 magenta  setCanDisplayReactToButtons() returned
///   LED 5 cyan     reached the event loop
///   LED 6 red      pulsing heartbeat, one per second
///
/// The last lit LED is the last call that completed. All seven lit alongside a
/// blank screen means the app is healthy and the fault is in display output.
///
/// Deliberately imports nothing beyond what hello.cpp proved works, plus the
/// calls actually being tested. An earlier version used printInt(),
/// terminalWrite() and showDialogMsgBox(); this firmware does not provide at
/// least one of those, and an unresolved import stops the module from
/// instantiating at all - so that version never ran a single line.

#include <fwwasm.h>

#include <cstdint>

namespace {

void stage(int led, int r, int g, int b) {
    // A long duration keeps the LED lit rather than fading, so the final
    // pattern is still readable a minute later.
    setBoardLED(led, r, g, b, 60000, ledsimplevalue);
    waitms(250);
}

void beep(float hz) { playSoundFromFrequencyAndDuration(hz, 0.12F, 0.2F, WAVETYPE_SINE); }

}  // namespace

int main() {
    // Stage 0: alive. Nothing before this line can have failed.
    stage(0, 255, 255, 255);
    beep(880.0F);

    // Stage 1: create a panel.
    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);
    stage(1, 0, 0, 255);

    // Stage 2: text, at the pixel size the vendor radio example uses.
    addControlText(1, 1, 20, 80, 1, 48, 255, 255, 255, "PROBE");
    addControlText(1, 2, 20, 140, 1, 24, 255, 255, 0, "if you can read this");
    stage(2, 0, 255, 0);

    // Stage 3: make it visible.
    showPanel(1);
    stage(3, 255, 255, 0);
    beep(1320.0F);

    // No setCanDisplayReactToButtons() call: this firmware does not provide it,
    // and importing it alone is enough to stop the module instantiating.

    // Stage 4: into the loop.
    stage(4, 0, 255, 255);

    // Stage 6: heartbeat. Pulsing forever means the app is healthy.
    uint8_t event_data[FW_GET_EVENT_DATA_MAX] = {0};
    while (true) {
        setBoardLED(6, 255, 0, 0, 500, ledpulsefade);

        // A beep per button press confirms input works even if nothing draws.
        while (hasEvent() != 0) {
            static_cast<void>(getEventData(event_data));
            beep(660.0F);
        }

        waitms(1000);
    }

    return 0;
}
