/// Rebuilds the hunt screen one piece at a time, to find what kills it.
///
/// The full screen runs, shows a correct reading, and then dies minutes later.
/// Patching individual suspects has not worked, so this starts from the least
/// the screen can possibly do and adds one thing per step - the same approach
/// that found every other problem on this board.
///
///   HSTEP  adds
///     1    read RSSI at 4 Hz and show the number. Nothing else.
///     2    a bargraph, updated only when the value changes
///     3    the seven-LED signal bar
///     4    audio chirps
///     5    the warmer/colder text
///     6    the peak/low text line, and 10 Hz sampling
///
/// Every step keeps a heartbeat LED, one write a second, so a dead app is
/// obvious. Leave each running for several minutes: the failure takes a while
/// to arrive, and a step that survives five minutes has passed.
///
/// Step 1 is the honest floor. If that dies, the problem is polling the radio
/// at all, and nothing built on top of it can be made to work.

#include <fwwasm.h>

#include <cstdint>

#ifndef HSTEP
#error "HSTEP must be defined (1-6)"
#endif

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

namespace {

constexpr int kRadio = 1;

#if HSTEP >= 6
constexpr uint32_t kPeriodMs = 100U;  // 10 Hz, as the app runs
#else
constexpr uint32_t kPeriodMs = 250U;  // 4 Hz
#endif

constexpr int kBeatEvery = static_cast<int>(1000U / kPeriodMs);

// Control indices.
constexpr int kCtlTitle = 0;
constexpr int kCtlNumber = 1;
#if HSTEP >= 2
constexpr int kCtlBar = 2;
#endif
#if HSTEP >= 5
constexpr int kCtlTrend = 3;
#endif
#if HSTEP >= 6
constexpr int kCtlPeak = 4;
#endif

#if HSTEP >= 3
constexpr int kBoardLeds = 7;
#endif

/// Dim, matching the app.
void led(int index, int r, int g, int b, LEDManagerLEDMode mode) {
    setBoardLED(index, r / 5, g / 5, b / 5, 400, mode);
}

#if HSTEP >= 2
/// Signal as 0-100 across -110..-40 dBm, the app's scale.
int level_percent(int dbm) {
    int pct = ((dbm + 110) * 100) / 70;
    if (pct < 0) {
        pct = 0;
    }
    if (pct > 100) {
        pct = 100;
    }
    return pct;
}
#endif

}  // namespace

int main() {
    led(0, 0, 255, 0, ledsimplevalue);

    addPanel(1, 1, 0, 0, 0, 0, 0, 0, 1);
    addControlText(1, kCtlTitle, 8, 4, 1, 26, 255, 176, 0, "HUNT " STRINGIFY(HSTEP));
    addControlNumber(1, kCtlNumber, 1, 14, 50, 200, 3, 0, 255, 255, 255, 0, 0, 0, 0);
#if HSTEP >= 2
    addControlBargraph(1, kCtlBar, 1, 14, 110, 292, 34, 0, 100, 0, 230, 90);
#endif
#if HSTEP >= 5
    addControlText(1, kCtlTrend, 14, 160, 1, 34, 255, 255, 255, " ");
#endif
#if HSTEP >= 6
    addControlText(1, kCtlPeak, 14, 210, 0, 16, 140, 150, 160, " ");
#endif
    showPanel(1);

    // Open the receiver once. No IDLE/RX cycling anywhere in this file.
    RadioSetRx(kRadio);

    led(1, 0, 0, 255, ledsimplevalue);

    // Smoothing, so the displayed value is not raw noise. Pure arithmetic, no
    // host calls, so it cannot be what fails.
    int32_t fast = 0;
    int32_t slow = 0;
    bool have = false;

    // Last values written, so nothing is sent twice.
    int last_dbm = -999;
#if HSTEP >= 2
    int last_pct = -1;
#endif
#if HSTEP >= 3
    int last_lit = -1;
#endif
#if HSTEP >= 5
    int last_trend = -99;
#endif
    int beat = 0;

    while (true) {
        const int dbm = RadioGetRSSI(kRadio);

        const int32_t sample = static_cast<int32_t>(dbm) * 256;
        if (!have) {
            fast = sample;
            slow = sample;
            have = true;
        } else {
            fast += (sample - fast) >> 2;
            slow += (sample - slow) >> 5;
        }
        const int shown = static_cast<int>(fast / 256);

        if (shown != last_dbm) {
            setControlValue(1, kCtlNumber, shown);
            last_dbm = shown;
        }

#if HSTEP >= 2
        const int pct = level_percent(shown);
        if (pct != last_pct) {
            setControlValue(1, kCtlBar, pct);
            last_pct = pct;
        }
#endif

#if HSTEP >= 3
        const int lit = (level_percent(shown) * kBoardLeds) / 100;
        if (lit != last_lit) {
            const int from = (last_lit < 0) ? 0 : (lit < last_lit ? lit : last_lit);
            const int to = (last_lit < 0) ? kBoardLeds : (lit > last_lit ? lit : last_lit);
            for (int i = from; i < to && i < kBoardLeds; ++i) {
                if (i < lit) {
                    const int ramp = (i * 255) / (kBoardLeds - 1);
                    led(i, ramp, 255 - ramp, 0, ledsimplevalue);
                } else {
                    led(i, 0, 0, 0, ledsimplevalue);
                }
            }
            last_lit = lit;
        }
#endif

#if HSTEP >= 4
        // Chirp roughly twice a second, pitch following the level.
        if ((beat % 2) == 0) {
            playSoundFromFrequencyAndDuration(static_cast<float>(400 + (level_percent(shown) * 20)),
                                              0.04F, 0.2F,
                                              WAVETYPE_SINE);
        }
#endif

#if HSTEP >= 5
        const int32_t delta = fast - slow;
        const int trend = (delta > 256) ? 1 : ((delta < -256) ? -1 : 0);
        if (trend != last_trend) {
            setControlValueText(1, kCtlTrend,
                                (trend > 0) ? ">>> WARMER"
                                            : ((trend < 0) ? "<<< colder" : "--- steady"));
            last_trend = trend;
        }
#endif

#if HSTEP >= 6
        if ((beat % 8) == 0) {
            setControlValueText(1, kCtlPeak, "sampling at 10 Hz");
        }
#endif

        if ((beat % kBeatEvery) == 0) {
            led(6, 255, 0, 0, ledpulsefade);
        }

        ++beat;
        waitms(static_cast<int>(kPeriodMs));
    }

    return 0;
}
