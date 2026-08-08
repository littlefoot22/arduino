/// Import bisector: finds which host function this firmware does not provide.
///
/// A WebAssembly module that imports a function the runtime cannot resolve
/// fails at instantiation. Nothing runs, and this device reports nothing when
/// it happens - the same blank screen as every other failure mode.
///
/// hello.cpp runs on this hardware and imports six host functions. probe.cpp
/// imports twelve and does not run. So the question is which of the extra ones
/// is missing, and the only way to ask is one group at a time.
///
/// Each build of this file is hello.cpp plus one group of imports, referenced
/// behind a volatile flag that is never true. The functions therefore appear in
/// the module's import section - which is all it takes to break instantiation -
/// but are never actually executed. So:
///
///   shows "TIER n"  -> every import in that group exists
///   blank screen    -> at least one import in that group is missing
///
/// Build via the imports1..imports5 CMake targets, which set TIER.

#include <fwwasm.h>

#include <cstdint>

#ifndef TIER
#error "TIER must be defined (1-5)"
#endif

namespace {

/// Never becomes non-zero. Volatile so the optimiser cannot prove that and
/// delete the calls, which would also delete the imports we are testing.
volatile int g_never = 0;

#if TIER == 1
constexpr const char* kLabel = "TIER 1 diag";
char g_term[] = "x";
#elif TIER == 2
constexpr const char* kLabel = "TIER 2 appctl";
#elif TIER == 3
constexpr const char* kLabel = "TIER 3 events";
#elif TIER == 4
constexpr const char* kLabel = "TIER 4 controls";
#elif TIER == 5
constexpr const char* kLabel = "TIER 5 radio";
#endif

/// References every import in this tier without ever running them.
void touch_imports() {
    if (g_never == 0) {
        return;
    }

#if TIER == 1
    // Diagnostics and dialogs.
    printInt("x", printColorNormal, printInt32, 0);
    terminalWrite(g_term);
    showDialogMsgBox("x", 1, 0, 0, 0, 0);
#elif TIER == 2
    // Application and panel control.
    setCanDisplayReactToButtons(4);
    setPanelMenuText(1, 0, "x");
    exitToMainAppMenu();
#elif TIER == 3
    // Event queue.
    unsigned char data[FW_GET_EVENT_DATA_MAX] = {0};
    if (hasEvent() != 0) {
        getEventData(data);
    }
#elif TIER == 4
    // The richer control types.
    addControlNumber(1, 5, 1, 0, 0, 100, 3, 0, 255, 255, 255, 0, 0, 0, 0);
    addControlBargraph(1, 6, 1, 0, 0, 100, 20, 0, 100, 0, 255, 0);
    setControlValue(1, 6, 50);
    setControlValueText(1, 5, "x");
#elif TIER == 5
    // Radio. RadioLoadConfig is the least documented call in the whole API and
    // a prime candidate for not existing in older firmware.
    unsigned char config[4] = {0};
    RadioLoadConfig(1, config, 4);
    RadioSetIdle(1);
    RadioSetRx(1);
    RadioGetRSSI(1);
#endif
}

}  // namespace

int main() {
    // Identical opening to hello.cpp, which is known to run.
    setBoardLED(0, 0, 255, 0, 60000, ledsimplevalue);
    playSoundFromFrequencyAndDuration(880.0F, 0.15F, 0.2F, WAVETYPE_SINE);

    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);
    addControlText(1, 1, 20, 90, 1, 48, 255, 255, 255, kLabel);
    showPanel(1);

    setBoardLED(1, 0, 0, 255, 60000, ledsimplevalue);

    touch_imports();

    while (true) {
        waitms(1000);
    }

    return 0;
}
