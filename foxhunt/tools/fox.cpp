/// One hunt app per fox. Single screen, single frequency, nothing else.
///
/// This replaces the multi-panel app. Every piece of the hunt screen survived
/// on its own (tools/hunt.cpp, all six steps), so what was killing it was the
/// structure around it - four panels, screen switching, a deferred-request
/// machine - not the meter, the radio or the display. Rather than keep hunting
/// that, the structure is gone: no panel switching, no state machine, no fox
/// selection. Pick your fox by choosing which file to run.
///
///   red    exit
///
/// Silent. There is no audio and no sound import at all - a beacon that chirps
/// at you continuously is not something you want strapped to your chest for an
/// hour, and the screen and LEDs already say everything the pitch did.
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

/// One measurement per second, each preceded by its own IDLE/RX cycle.
///
/// Measured on hardware: the reading only changed every thirty seconds, which
/// was exactly the old full-flush interval. Calling RadioSetRx on a receiver
/// that has stopped updating does nothing; only dropping to IDLE and back
/// re-arms it. So the RSSI register carries one fresh value per RX entry, and
/// a meter that updates needs a cycle per reading rather than a fast poll of a
/// register that is not moving.
///
/// One second is the cautious end. Flushing every few hundred milliseconds hung
/// this board once, so this sits several times clear of that. If it proves
/// solid, kPeriodMs is the one number to lower.
constexpr uint32_t kPeriodMs = 1000U;
constexpr int kBeatEvery = 1;

/// Settling time after re-entering RX, before the reading means anything.
constexpr int kIdleSettleMs = 20;
constexpr int kRxSettleMs = 60;

constexpr int kBoardLeds = 7;

constexpr int kCtlName = 0;
constexpr int kCtlFreq = 1;
constexpr int kCtlDbm = 2;
constexpr int kCtlBar = 3;
constexpr int kCtlTrend = 4;
constexpr int kCtlHint = 5;

const Fox& fox() { return kFoxes[FOX_INDEX]; }

void led(int index, int r, int g, int b, LEDManagerLEDMode mode) {
    // 20 percent. These sit close to your face in the dark.
    setBoardLED(index, r / 5, g / 5, b / 5, 400, mode);
}

/// Sets the frequency, and tells the receiver to stay in RX.
///
/// Ten bytes of address/value pairs. Still far below the fifty-six that
/// hard-reset the board, and only a little above the six that step10 passed
/// without trouble.
///
/// The two MCSM registers matter as much as the frequency here. By default the
/// CC1101 leaves RX the moment it believes it has received something -
/// RXOFF_MODE is IDLE - and once it is out of RX the RSSI register stops
/// updating and simply holds its last value. That is exactly the symptom of a
/// reading that moves for a while, settles, and never shifts again until the
/// script is restarted.
bool tune_once() {
    const uint32_t word = cc1101::freq_to_word(fox().freq_hz);
    unsigned char config[10] = {
        0x0D, static_cast<unsigned char>((word >> 16) & 0xFFU),  // FREQ2
        0x0E, static_cast<unsigned char>((word >> 8) & 0xFFU),   // FREQ1
        0x0F, static_cast<unsigned char>(word & 0xFFU),          // FREQ0
        0x16, 0x07,  // MCSM2: RX_TIME = 111, receive without timing out
        0x17, 0x3C,  // MCSM1: RXOFF_MODE = 11, stay in RX afterwards
    };
    return RadioLoadConfig(kRadio, config, 10) != 0;
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
    addControlText(1, kCtlHint, 14, 220, 0, 14, 140, 150, 160, "red: exit");
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
    bool exit_now = false;
    int beat = 0;

    uint8_t event_data[FW_GET_EVENT_DATA_MAX] = {0};

    while (!exit_now) {
        // Bounded drain. An open while() here locked the board once.
        for (int drained = 0; drained < 8 && hasEvent() != 0; ++drained) {
            if (getEventData(event_data) == FWGUI_EVENT_RED_BUTTON) {
                exit_now = true;
            }
        }
        if (exit_now) {
            break;
        }

        // Re-arm, settle, then read. The whole cycle is the measurement.
        RadioSetIdle(kRadio);
        waitms(kIdleSettleMs);
        RadioSetRx(kRadio);
        waitms(kRxSettleMs);

        const int dbm = cc1101::normalize_rssi(RadioGetRSSI(kRadio));

        const int32_t sample = static_cast<int32_t>(dbm) * 256;
        if (!have) {
            fast = sample;
            slow = sample;
            have = true;
        } else {
            // Lighter smoothing than before. At four readings a second a heavy
            // filter was free; at one, a shift of two would take four seconds
            // to catch up with a signal that had already changed.
            fast += (sample - fast) >> 1;
            slow += (sample - slow) >> 3;
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
