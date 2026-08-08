/// Cumulative ladder from a known-good hello world up to everything the
/// foxhunt app needs, adding exactly one host call per step.
///
/// Flash them in order. The first step that shows a blank screen names the call
/// that breaks this firmware - it is whatever that step added.
///
/// Two different failures are possible and the LEDs tell them apart:
///
///   nothing at all, no green LED   the module never instantiated, so the new
///                                  call is an import this firmware lacks
///   green LED but no blue          the module runs but the new call hangs or
///                                  traps partway through setup
///   green + blue + text            that step is fine, move to the next
///
///   STEP  adds
///     1   baseline: addPanel, addControlText, showPanel  (known good)
///     2   setCanDisplayReactToButtons
///     3   hasEvent
///     4   getEventData
///     5   setPanelMenuText
///     6   addControlBargraph, setControlValue
///     7   addControlNumber
///     8   setControlValueText
///     9   exitToMainAppMenu   (imported only, never called)
///    10   RadioSetIdle, RadioSetRx, RadioGetRSSI
///    11   RadioLoadConfig
///
/// From step 10 the measured RSSI is displayed live, so a passing step 10 also
/// confirms the receiver works.

#include <fwwasm.h>

#include <cstdint>

#ifndef STEP
#error "STEP must be defined (1-11)"
#endif

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

namespace {

constexpr const char* kLabel = "STEP " STRINGIFY(STEP);

/// Control indices, deliberately kept low and contiguous. Every vendor example
/// stays under ten, and a firmware with a fixed per-panel array would be
/// another way to produce a blank screen.
constexpr int kCtlLabel = 1;
#if STEP >= 6
constexpr int kCtlBar = 2;
#endif
#if STEP >= 7
constexpr int kCtlNumber = 3;
#endif
#if STEP >= 8
constexpr int kCtlStatus = 4;
#endif

#if STEP >= 9
/// Never true. Volatile so the optimiser cannot delete the guarded call, which
/// would also delete the import under test.
volatile int g_never = 0;
#endif

void led(int index, int r, int g, int b) { setBoardLED(index, r, g, b, 60000, ledsimplevalue); }

}  // namespace

int main() {
    // Proof of life before anything touches the display.
    led(0, 0, 255, 0);
    playSoundFromFrequencyAndDuration(880.0F, 0.15F, 0.2F, WAVETYPE_SINE);

    // ---- step 1: the hello baseline, known to run on this hardware ----
    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);
    addControlText(1, kCtlLabel, 20, 40, 1, 40, 255, 255, 255, kLabel);
    showPanel(1);

#if STEP >= 5
    // ---- step 5: button captions ----
    setPanelMenuText(1, 0, "A");
    setPanelMenuText(1, 1, "B");
#endif

#if STEP >= 6
    // ---- step 6: a bargraph, and setting its value ----
    addControlBargraph(1, kCtlBar, 1, 20, 110, 280, 30, 0, 100, 0, 255, 0);
    setControlValue(1, kCtlBar, 50);
#endif

#if STEP >= 7
    // ---- step 7: a numeric readout ----
    addControlNumber(1, kCtlNumber, 1, 20, 150, 200, 3, 0, 255, 255, 255, 0, 0, 0, 0);
    setControlValue(1, kCtlNumber, -99);
#endif

#if STEP >= 8
    // ---- step 8: updating text after creation ----
    addControlText(1, kCtlStatus, 20, 200, 1, 20, 200, 200, 200, "");
    setControlValueText(1, kCtlStatus, "text updated");
#endif

#if STEP >= 2
    // ---- step 2: hand the buttons to the app ----
    // Placed after the screen is already drawn, so if this call is the problem
    // the text above will have appeared first and the LEDs will show where it
    // stopped.
    setCanDisplayReactToButtons(4);
#endif

#if STEP >= 9
    // ---- step 9: imported but never called, since calling it would exit ----
    if (g_never != 0) {
        exitToMainAppMenu();
    }
#endif

#if STEP >= 10
    // ---- step 10: bring the receiver up on a fox frequency ----
    RadioSetIdle(1);
    RadioSetRx(1);
    static_cast<void>(RadioGetRSSI(1));
#endif

#if STEP >= 11
    // ---- step 11: the least documented call in the API ----
    // A minimal register write: FREQ2/FREQ1/FREQ0 for 446.025 MHz as
    // address/value pairs. Whether this is the format the firmware wants is
    // exactly what is being tested.
    unsigned char config[6] = {0x0D, 0x11, 0x0E, 0x2B, 0x0F, 0x62};
    static_cast<void>(RadioLoadConfig(1, config, 6));
    RadioSetRx(1);
#endif

    // Every setup call above returned.
    led(1, 0, 0, 255);

#if STEP >= 4
    uint8_t event_data[FW_GET_EVENT_DATA_MAX] = {0};
#endif

    while (true) {
#if STEP >= 3
        // ---- step 3: poll the event queue ----
        while (hasEvent() != 0) {
#if STEP >= 4
            // ---- step 4: drain it ----
            static_cast<void>(getEventData(event_data));
            playSoundFromFrequencyAndDuration(660.0F, 0.05F, 0.2F, WAVETYPE_SINE);
#else
            // Without getEventData the queue never empties, so do not spin.
            break;
#endif
        }
#endif

#if STEP >= 10
        // Show live signal strength. RadioGetRSSI reports dBm directly on this
        // firmware if the value is negative; a positive value is the raw
        // register and is left alone here so the reading stays honest.
        const int rssi = RadioGetRSSI(1);
#if STEP >= 7
        setControlValue(1, kCtlNumber, rssi);
#endif
#endif

        // Heartbeat, so a live app is distinguishable from a frozen one.
        setBoardLED(6, 255, 0, 0, 400, ledpulsefade);
        waitms(500);
    }

    return 0;
}
