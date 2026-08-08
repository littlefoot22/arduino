/// Diagnostic probe for a FreeWili that shows a blank screen.
///
/// Lights one board LED per startup stage before attempting that stage, so the
/// LEDs report how far the app got even when nothing draws. LEDs and sound do
/// not depend on the display pipeline at all, which is exactly what we need
/// when the display is the thing under suspicion.
///
///   LED 0 white    main() entered - the module loaded and is executing
///   LED 1 blue     setCanDisplayReactToButtons() returned
///   LED 2 green    addPanel() returned
///   LED 3 yellow   addControlText() returned
///   LED 4 magenta  showPanel() returned
///   LED 5 cyan     reached the event loop
///   LED 6 red      pulsing heartbeat, one per second
///
/// Read it as: the last lit LED is the last call that completed. All seven lit
/// plus a blank screen means the app runs fine and the problem is confined to
/// how panels are displayed.
///
/// Everything here mirrors the vendor radio example's call pattern as closely
/// as possible, including passing 4 to setCanDisplayReactToButtons().

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

// terminalWrite() takes a mutable char*, so these cannot be string literals.
char kMsgEntered[] = "PROBE: main() entered\n";
char kMsgAlive[] = "PROBE: alive\n";

}  // namespace

int main() {
    // Stage 0: we are alive. Nothing before this line can have failed.
    stage(0, 255, 255, 255);
    beep(880.0F);

    printInt("\nPROBE: main() entered\n", printColorGreen, printInt32, 0);
    terminalWrite(kMsgEntered);

    // Stage 1: button handling. The vendor example passes 4 here; the main app
    // passes 0, and this is one of the differences worth ruling out.
    setCanDisplayReactToButtons(4);
    stage(1, 0, 0, 255);
    printInt("PROBE: buttons configured\n", printColorGreen, printInt32, 0);

    // Stage 2: create a panel. Black background, menu bar on, exactly as the
    // vendor example does it.
    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);
    stage(2, 0, 255, 0);
    printInt("PROBE: addPanel done\n", printColorGreen, printInt32, 0);

    // Stage 3: one big white string. 64 is the pixel size the vendor radio
    // example uses, so if text is going to show at all it should show here.
    addControlText(1, 1, 20, 90, 1, 64, 255, 255, 255, "PROBE");
    addControlText(1, 2, 20, 150, 1, 24, 255, 255, 0, "if you can read this");
    stage(3, 255, 255, 0);
    printInt("PROBE: addControlText done\n", printColorGreen, printInt32, 0);

    // Stage 4: make it visible.
    showPanel(1);
    stage(4, 255, 0, 255);
    beep(1320.0F);
    printInt("PROBE: showPanel done\n", printColorGreen, printInt32, 0);

    // A modal dialog renders through a different path than panel controls, so
    // if panels are broken but this appears, that narrows it a long way.
    showDialogMsgBox("PROBE DIALOG", 1, 0, 0, 0, 0);

    // Stage 5: into the loop.
    stage(5, 0, 255, 255);
    printInt("PROBE: entering loop\n", printColorGreen, printInt32, 0);

    // Stage 6: heartbeat. If this pulses forever, the app is healthy and the
    // display is the only thing at fault.
    uint32_t beat = 0;
    uint8_t event_data[FW_GET_EVENT_DATA_MAX] = {0};
    while (true) {
        setBoardLED(6, 255, 0, 0, 500, ledpulsefade);
        printInt("PROBE: beat %d\n", printColorBlue, printUInt32, static_cast<int>(beat));
        terminalWrite(kMsgAlive);

        // Report any button presses, which confirms input works even if the
        // screen never draws.
        while (hasEvent() != 0) {
            const int event = getEventData(event_data);
            printInt("PROBE: event %d\n", printColorRed, printInt32, event);
            beep(660.0F);
        }

        ++beat;
        waitms(1000);
    }

    return 0;
}
