/// One hunt app per fox. Single screen, single frequency, nothing else.
///
/// This replaces the multi-panel app. Every piece of the hunt screen survived
/// on its own (tools/hunt.cpp, all six steps), so what was killing it was the
/// structure around it - four panels, screen switching, a deferred-request
/// machine - not the meter, the radio or the display. Rather than keep hunting
/// that, the structure is gone: no panel switching, no state machine, no fox
/// selection. Pick your fox by choosing which file to run.
///
///   gray   mute or unmute the chirps
///   red    exit
///
/// Tuning is attempted with a six-byte register write, and only once at
/// startup. That size is the one measurement available: tools/step.cpp passed
/// six bytes to RadioLoadConfig without trouble, while the old app passed
/// fifty-six and hard-reset the board. If FREQ shows "set by hand" the write
/// was refused, and the frequency has to come from the board's Radio panel
/// instead - everything else still works.

#include <fwwasm.h>

#include <cstdint>

#include "cc1101.h"
#include "df.h"
#include "foxes.h"

#ifndef FOX_INDEX
#error "FOX_INDEX must be defined"
#endif

namespace {

using namespace foxhunt;

constexpr int kRadio = 1;

/// 4 Hz. Proven to run indefinitely by the hunt ladder, and the smoothing
/// filter governs how fast the needle moves anyway.
constexpr uint32_t kPeriodMs = 250U;
constexpr int kBeatEvery = static_cast<int>(1000U / kPeriodMs);

constexpr int kBoardLeds = 7;

constexpr int kCtlName = 0;
constexpr int kCtlFreq = 1;
constexpr int kCtlDbm = 2;
constexpr int kCtlBar = 3;
constexpr int kCtlTrend = 4;
constexpr int kCtlAudio = 5;

const Fox& fox() { return kFoxes[FOX_INDEX]; }

void led(int index, int r, int g, int b, LEDManagerLEDMode mode) {
    // 20 percent. These sit close to your face in the dark.
    setBoardLED(index, r / 5, g / 5, b / 5, 400, mode);
}

/// Writes just FREQ2, FREQ1 and FREQ0 as address/value pairs.
///
/// The smallest thing that could possibly retune the part, and the only size
/// with any evidence behind it.
bool tune_once() {
    const uint32_t word = cc1101::freq_to_word(fox().freq_hz);
    unsigned char config[6] = {
        0x0D, static_cast<unsigned char>((word >> 16) & 0xFFU),
        0x0E, static_cast<unsigned char>((word >> 8) & 0xFFU),
        0x0F, static_cast<unsigned char>(word & 0xFFU),
    };
    return RadioLoadConfig(kRadio, config, 6) != 0;
}

/// Builds "446.025 MHz" without pulling in any of libc.
class Buf {
public:
    Buf() { data_[0] = '\0'; }

    void mhz_line(uint32_t hz, bool tuned) {
        len_ = 0;
        num(static_cast<int>(hz / 1000000U));
        ch('.');
        num(static_cast<int>((hz % 1000000U) / 1000U), 3);
        str(tuned ? " MHz" : " MHz  set by hand");
    }

    const char* c_str() const { return data_; }

private:
    void ch(char c) {
        if (len_ < 47) {
            data_[len_] = c;
            ++len_;
            data_[len_] = '\0';
        }
    }
    void str(const char* text) {
        while (*text != '\0') {
            ch(*text);
            ++text;
        }
    }
    void num(int value, int pad = 0) {
        char digits[12];
        int count = 0;
        do {
            digits[count] = static_cast<char>('0' + (value % 10));
            ++count;
            value /= 10;
        } while (value != 0 && count < 12);
        for (int i = count; i < pad; ++i) {
            ch('0');
        }
        for (int i = count - 1; i >= 0; --i) {
            ch(digits[i]);
        }
    }

    char data_[48];
    int len_ = 0;
};

Buf g_buf;

}  // namespace

int main() {
    led(0, 0, 255, 0, ledsimplevalue);

    addPanel(1, 1, 0, 0, 0, 8, 12, 18, 1);
    addControlText(1, kCtlName, 8, 4, 1, 30, 255, 176, 0, fox().name);
    addControlText(1, kCtlFreq, 8, 44, 0, 16, 140, 150, 160, " ");
    addControlNumber(1, kCtlDbm, 1, 14, 70, 200, 3, 0, 255, 255, 255, 0, 0, 0, 0);
    addControlBargraph(1, kCtlBar, 1, 14, 130, 292, 34, 0, 100, 0, 230, 90);
    addControlText(1, kCtlTrend, 14, 176, 1, 34, 255, 255, 255, " ");
    addControlText(1, kCtlAudio, 14, 220, 0, 14, 140, 150, 160, "gray: mute    red: exit");
    setPanelMenuText(1, 0, "MUTE");
    setPanelMenuText(1, 4, "EXIT");
    showPanel(1);

    const bool tuned = tune_once();
    RadioSetRx(kRadio);

    g_buf.mhz_line(fox().freq_hz, tuned);
    setControlValueText(1, kCtlFreq, g_buf.c_str());

    led(1, 0, 0, 255, ledsimplevalue);

    int32_t fast = 0;
    int32_t slow = 0;
    bool have = false;

    int last_dbm = -999;
    int last_pct = -1;
    int last_lit = -1;
    int last_trend = -99;
    bool muted = false;
    bool exit_now = false;
    int beat = 0;

    uint8_t event_data[FW_GET_EVENT_DATA_MAX] = {0};

    while (!exit_now) {
        // Bounded drain. An open while() here locked the board once.
        for (int drained = 0; drained < 8 && hasEvent() != 0; ++drained) {
            const int event = getEventData(event_data);
            if (event == FWGUI_EVENT_GRAY_BUTTON) {
                muted = !muted;
                setControlValueText(1, kCtlAudio,
                                    muted ? "MUTED         red: exit"
                                          : "gray: mute    red: exit");
            } else if (event == FWGUI_EVENT_RED_BUTTON) {
                exit_now = true;
            }
        }
        if (exit_now) {
            break;
        }

        const int dbm = cc1101::normalize_rssi(RadioGetRSSI(kRadio));

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
        const int pct = df::level_percent(shown);

        if (shown != last_dbm) {
            setControlValue(1, kCtlDbm, shown);
            last_dbm = shown;
        }
        if (pct != last_pct) {
            setControlValue(1, kCtlBar, pct);
            last_pct = pct;
        }

        const int lit = (pct * kBoardLeds) / 100;
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

        const int32_t delta = fast - slow;
        const int trend = (delta > 256) ? 1 : ((delta < -256) ? -1 : 0);
        if (trend != last_trend) {
            setControlValueText(1, kCtlTrend, (trend > 0) ? ">>> WARMER"
                                                          : ((trend < 0) ? "<<< colder"
                                                                         : "--- steady"));
            last_trend = trend;
        }

        // Chirp twice a second, pitch following the level.
        if (!muted && (beat % 2) == 0) {
            playSoundFromFrequencyAndDuration(static_cast<float>(400 + (pct * 20)), 0.04F, 0.2F,
                                              WAVETYPE_SINE);
        }

        if ((beat % kBeatEvery) == 0) {
            led(6, 255, 0, 0, ledpulsefade);
        }

        ++beat;
        waitms(static_cast<int>(kPeriodMs));
    }

    // Leave the hardware quiet. exitToMainAppMenu() is not called - it crashed
    // the board every time. Returning from main() ends the script.
    RadioSetIdle(kRadio);
    for (int i = 0; i < kBoardLeds; ++i) {
        led(i, 0, 0, 0, ledsimplevalue);
    }
    return 0;
}
