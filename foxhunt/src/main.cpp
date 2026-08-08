/// Foxhunt direction finder for the FreeWili OG.
///
/// Tunes a CC1101 to a contest beacon, watches its RSSI, and turns that single
/// number into something you can actually walk with: a smoothed meter, a
/// warmer/colder trend, an audible pitch, and a body-shielding rotation scan
/// that yields a relative bearing.
///
/// Covers the 70 cm foxes only. See src/foxes.h for why the 2 m half of the
/// contest is unreachable on this hardware.

#include <fwwasm.h>

#include <cstdint>

#include "cc1101.h"
#include "df.h"
#include "foxes.h"
#include "ui.h"

namespace foxhunt {
namespace {

/// Radio 1 of the two CC1101s.
constexpr int kRadio = 1;

/// Sampling period. 20 Hz is fast enough that the meter tracks an antenna
/// swing, slow enough to leave the display processor time to repaint.
constexpr uint32_t kSampleMs = 50U;

/// Repaint every fourth sample; the screen cannot usefully show more.
constexpr int kFramesPerRepaint = 4;

/// How long one full body rotation should take.
constexpr uint32_t kSweepMs = 12000U;

/// Settling time after retuning before RSSI means anything. The synthesiser
/// recalibrates on the IDLE->RX transition and the AGC needs a moment to
/// converge on the new channel.
constexpr uint32_t kRetuneSettleMs = 30U;

/// Samples averaged per channel during a band scan.
constexpr int kScanSamplesPerChannel = 6;

/// Set false for a silent hunt.
constexpr bool kAudioEnabled = true;

enum class Screen : uint8_t { kSelect, kHunt, kRose, kScan };

struct App {
    Screen screen = Screen::kSelect;
    int fox_index = 0;
    int gain_step = 0;
    int bw_index = 0;
    bool tuned_ok = false;

    df::Meter meter;
    df::RotationScan scan;

    /// Milliseconds since start, accumulated from the loop period. The host API
    /// exposes no monotonic clock, and every consumer here measures elapsed
    /// time rather than absolute time, so a counted tick is sufficient.
    uint32_t now_ms = 0U;

    int frame = 0;
    uint32_t next_chirp_ms = 0U;
    bool should_exit = false;
};

App g_app;

/// Loads a receive configuration for `hz` and parks the radio in RX.
bool tune(uint32_t hz, int gain_step, int bw_index) {
    uint8_t config[cc1101::kConfigBytes];
    const size_t len = cc1101::build_rx_config(config, sizeof(config), hz, gain_step, bw_index);
    if (len == 0U) {
        return false;
    }

    RadioSetIdle(kRadio);
    if (RadioLoadConfig(kRadio, config, static_cast<int>(len)) == 0) {
        return false;
    }
    if (RadioSetRx(kRadio) == 0) {
        return false;
    }

    waitms(static_cast<int>(kRetuneSettleMs));
    return true;
}

/// Retunes to the selected fox and clears any now-stale signal history.
void retune_current() {
    g_app.tuned_ok = tune(kFoxes[g_app.fox_index].freq_hz, g_app.gain_step, g_app.bw_index);
    g_app.meter.reset();
    ui::set_hunt_status(g_app.tuned_ok ? "" : "radio config rejected");
}

int read_rssi_dbm() { return cc1101::normalize_rssi(RadioGetRSSI(kRadio)); }

/// Chirps at a pitch and rate set by signal strength.
void service_audio(int percent) {
    if (!kAudioEnabled || g_app.now_ms < g_app.next_chirp_ms) {
        return;
    }
    playSoundFromFrequencyAndDuration(static_cast<float>(df::tone_hz_for(percent)), 0.04F, 0.2F,
                                      WAVETYPE_SINE);
    g_app.next_chirp_ms =
        g_app.now_ms + static_cast<uint32_t>(df::chirp_interval_ms_for(percent));
}

/// Sweeps all 70 cm fox channels and reports which is loudest.
///
/// Worth running whenever you lose the trail: it answers "am I even chasing the
/// right fox?" before you spend twenty minutes walking the wrong bearing.
void run_band_scan() {
    int best_index = 0;
    int best_dbm = -200;
    int results[kFoxCount];
    bool measured[kFoxCount];

    for (int i = 0; i < kFoxCount; ++i) {
        results[i] = -200;
        measured[i] = false;

        if (!tune(kFoxes[i].freq_hz, g_app.gain_step, g_app.bw_index)) {
            ui::update_scan_row(i, 0, false, -1);
            continue;
        }

        // Peak-hold rather than mean: a beacon keying on and off would
        // otherwise average down to nothing.
        int peak = -200;
        for (int s = 0; s < kScanSamplesPerChannel; ++s) {
            const int dbm = read_rssi_dbm();
            if (dbm > peak) {
                peak = dbm;
            }
            waitms(static_cast<int>(kSampleMs));
        }

        results[i] = peak;
        measured[i] = true;
        if (peak > best_dbm) {
            best_dbm = peak;
            best_index = i;
        }
        ui::update_scan_row(i, peak, true, -1);
    }

    for (int i = 0; i < kFoxCount; ++i) {
        ui::update_scan_row(i, results[i], measured[i], best_index);
    }

    // Leave the radio where the hunt expects it.
    retune_current();
}

void enter_select() {
    g_app.screen = Screen::kSelect;
    ui::clear_leds();
    ui::show_select(g_app.fox_index);
}

void enter_hunt() {
    g_app.screen = Screen::kHunt;
    retune_current();
    ui::show_hunt();
}

void enter_rose() {
    g_app.screen = Screen::kRose;
    ui::clear_leds();
    ui::show_rose();
    ui::update_rose(g_app.scan, g_app.now_ms);
}

void enter_scan() {
    g_app.screen = Screen::kScan;
    ui::clear_leds();
    ui::show_scan();
    run_band_scan();
}

void handle_select_button(int event) {
    switch (event) {
        case FWGUI_EVENT_GRAY_BUTTON:
            g_app.fox_index = (g_app.fox_index + kFoxCount - 1) % kFoxCount;
            ui::update_select(g_app.fox_index);
            break;
        case FWGUI_EVENT_YELLOW_BUTTON:
            g_app.fox_index = (g_app.fox_index + 1) % kFoxCount;
            ui::update_select(g_app.fox_index);
            break;
        case FWGUI_EVENT_GREEN_BUTTON:
            enter_hunt();
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            enter_scan();
            break;
        case FWGUI_EVENT_RED_BUTTON:
            g_app.should_exit = true;
            break;
        default:
            break;
    }
}

void handle_hunt_button(int event) {
    switch (event) {
        case FWGUI_EVENT_GRAY_BUTTON:
            if (g_app.gain_step > 0) {
                --g_app.gain_step;
                retune_current();
            }
            break;
        case FWGUI_EVENT_YELLOW_BUTTON:
            if (g_app.gain_step < (cc1101::kGainSteps - 1)) {
                ++g_app.gain_step;
                retune_current();
            }
            break;
        case FWGUI_EVENT_GREEN_BUTTON:
            enter_rose();
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            enter_select();
            break;
        case FWGUI_EVENT_RED_BUTTON:
            g_app.should_exit = true;
            break;
        default:
            break;
    }
}

void handle_rose_button(int event) {
    switch (event) {
        case FWGUI_EVENT_GREEN_BUTTON:
            g_app.scan.begin(g_app.now_ms, kSweepMs);
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            g_app.scan.cancel();
            g_app.screen = Screen::kHunt;
            ui::show_hunt();
            break;
        case FWGUI_EVENT_RED_BUTTON:
            g_app.should_exit = true;
            break;
        default:
            break;
    }
}

void handle_scan_button(int event) {
    switch (event) {
        case FWGUI_EVENT_GREEN_BUTTON:
            run_band_scan();
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            enter_select();
            break;
        case FWGUI_EVENT_RED_BUTTON:
            g_app.should_exit = true;
            break;
        default:
            break;
    }
}

void pump_events() {
    uint8_t data[FW_GET_EVENT_DATA_MAX];
    while (hasEvent() != 0) {
        const int event = getEventData(data);
        switch (g_app.screen) {
            case Screen::kSelect:
                handle_select_button(event);
                break;
            case Screen::kHunt:
                handle_hunt_button(event);
                break;
            case Screen::kRose:
                handle_rose_button(event);
                break;
            case Screen::kScan:
                handle_scan_button(event);
                break;
            default:
                break;
        }
        if (g_app.should_exit) {
            return;
        }
    }
}

/// One sampling tick for whichever screen is live.
void service_screen() {
    if (g_app.screen == Screen::kSelect || g_app.screen == Screen::kScan) {
        return;
    }

    if (!g_app.tuned_ok) {
        return;
    }

    const int dbm = read_rssi_dbm();
    g_app.meter.push(dbm);

    if (g_app.screen == Screen::kRose && g_app.scan.active()) {
        g_app.scan.push(g_app.now_ms, dbm);
    }

    const int percent = g_app.meter.percent();

    if (g_app.screen == Screen::kHunt) {
        service_audio(percent);
    }

    ++g_app.frame;
    if ((g_app.frame % kFramesPerRepaint) != 0) {
        return;
    }

    if (g_app.screen == Screen::kHunt) {
        ui::update_hunt(g_app.fox_index, g_app.meter, g_app.gain_step, g_app.bw_index,
                        g_app.tuned_ok);
        ui::update_leds(percent);
    } else {
        ui::update_rose(g_app.scan, g_app.now_ms);
    }
}

}  // namespace
}  // namespace foxhunt

int main() {
    using namespace foxhunt;

    // Static constructors are not guaranteed to run under -nostdlib with
    // --no-entry, so bring every piece of mutable state up explicitly.
    g_app.screen = Screen::kSelect;
    g_app.fox_index = 0;
    g_app.gain_step = 0;
    g_app.bw_index = 0;
    g_app.tuned_ok = false;
    g_app.now_ms = 0U;
    g_app.frame = 0;
    g_app.next_chirp_ms = 0U;
    g_app.should_exit = false;
    g_app.meter.reset();
    g_app.scan.clear();

    // Deliberately NOT calling setCanDisplayReactToButtons() here.
    //
    // Measured on hardware with tools/step.cpp: step 1 runs, and step 2, whose
    // only addition is that call, does not. This firmware predates it. The
    // vendor radio example calls it, so that example cannot run here either -
    // fwwasm.h describes a newer API than this board implements.
    //
    // The cost is that the firmware may also act on button presses rather than
    // leaving them entirely to us. Events still arrive through hasEvent() and
    // getEventData() either way, so the app works; if the firmware steals a
    // button, the workaround is to pick a different one, not to restore this
    // call.

    ui::build_all();
    enter_select();

    while (!g_app.should_exit) {
        pump_events();
        if (g_app.should_exit) {
            break;
        }
        service_screen();
        waitms(static_cast<int>(kSampleMs));
        g_app.now_ms += kSampleMs;
    }

    RadioSetIdle(kRadio);
    ui::clear_leds();
    exitToMainAppMenu();
    return 0;
}
