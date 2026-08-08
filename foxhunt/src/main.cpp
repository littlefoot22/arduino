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

/// Whether to send a register blob to the radio at all.
///
/// OFF, because doing so hard-resets this board. The app passed a 56-byte
/// address/value list and the device dropped USB mid-scan; step10 passed six
/// bytes to the same call and was fine, which points at a fixed-size buffer in
/// firmware rather than at the contents. RadioLoadConfig() is documented @todo
/// with no vendor example, so its expected size and layout are both guesses.
///
/// With this off the app reads RSSI on whatever frequency the radio is already
/// tuned to - set it from the board's own Radio panel - so the meter, the
/// rotation scan and the audio all work. What it cannot do is change channel,
/// which is what the band scan needs.
///
/// tools/radiocfg.cpp finds the largest size the call survives. Turn this back
/// on once that is known, and set kConfigMaxBytes to match.
constexpr bool kUseRadioConfig = false;

enum class Screen : uint8_t { kSelect, kHunt, kRose, kScan };

/// Work a button asks for, carried out later by the main loop.
///
/// Button handlers must not do the work themselves. Doing so put the radio
/// calls six frames deep - main, pump_events, the handler, enter_scan,
/// run_band_scan, tune - and the interpreter trapped with a stack overflow.
/// Every sequence that ran cleanly during bring-up called the radio from close
/// to main(), so depth, not the calls themselves, is what this device minds.
enum class Request : uint8_t { kNone, kSelect, kHunt, kRose, kScan, kRescan };

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

    Request pending = Request::kNone;

    /// Most recent event that matched none of the button codes, and how many
    /// such events have arrived.
    int last_odd_event = -1;
    int odd_events = 0;

    /// Band scan progress. The sweep runs one reading per main-loop pass rather
    /// than in a nested loop, which keeps the radio calls shallow and leaves
    /// the app responsive instead of frozen for the second and a half it takes.
    bool scan_running = false;
    int scan_index = 0;
    int scan_sample = 0;
    int scan_peak = -200;
    int scan_results[kFoxCount] = {};
    bool scan_measured[kFoxCount] = {};
};

App g_app;

/// Flashes the LED matching a received event, so button delivery can be
/// observed without a working log channel.
///
/// The firmware handles buttons itself on this board, since
/// setCanDisplayReactToButtons() does not exist here to stop it. Whether
/// presses ALSO reach the app is the open question, and this answers it:
///
///   LED 0 white    gray      LED 3 blue     blue
///   LED 1 yellow   yellow    LED 4 red      red
///   LED 2 green    green     LED 5 magenta  some other event
void flash_event_led(int event) {
    switch (event) {
        case FWGUI_EVENT_GRAY_BUTTON:
            ui::board_led(0, 255, 255, 255, 400, ledflash);
            break;
        case FWGUI_EVENT_YELLOW_BUTTON:
            ui::board_led(1, 255, 255, 0, 400, ledflash);
            break;
        case FWGUI_EVENT_GREEN_BUTTON:
            ui::board_led(2, 0, 255, 0, 400, ledflash);
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            ui::board_led(3, 0, 0, 255, 400, ledflash);
            break;
        case FWGUI_EVENT_RED_BUTTON:
            ui::board_led(4, 255, 0, 0, 400, ledflash);
            break;
        default:
            ui::board_led(5, 255, 0, 255, 400, ledflash);
            break;
    }
}

/// Scratch space for the register blob.
///
/// At file scope rather than on the stack. It is the largest frame in the app,
/// and this device traps on stack depth readily enough that keeping the deepest
/// call slim is worth one global.
uint8_t g_config[cc1101::kConfigBytes];

/// What the radio actually said when asked to retune.
///
/// RadioLoadConfig() is the one call in the API documented as @todo, with no
/// vendor example to copy, so the register blob format is an inference. Its
/// return code is recorded rather than acted on: a failure here used to abort
/// the measurement entirely, which produced a scan of empty rows and no clue
/// why. Reading RSSI from a radio that ignored the config still yields a
/// number, and seeing that number is what tells us whether the config took.
struct TuneResult {
    bool built = false;  ///< the blob was generated
    int cfg_rc = -1;     ///< RadioLoadConfig() return
    int rx_rc = -1;      ///< RadioSetRx() return
};

TuneResult g_last_tune;

/// Loads a receive configuration for `hz` and parks the radio in RX.
///
/// Carries on past a rejected config deliberately - see TuneResult.
TuneResult tune(uint32_t hz, int gain_step, int bw_index) {
    TuneResult result;

    const size_t len =
        cc1101::build_rx_config(g_config, sizeof(g_config), hz, gain_step, bw_index);
    result.built = (len != 0U);

    if (!kUseRadioConfig) {
        // Nothing to load, so touch the radio as little as possible. Cycling
        // IDLE/RX five times per sweep achieved nothing except giving the
        // driver more chances to fall over, which is what a rescan did. The
        // receiver is put into RX once at startup and left there.
        g_last_tune = result;
        return result;
    }

    RadioSetIdle(kRadio);
    if (result.built) {
        result.cfg_rc = RadioLoadConfig(kRadio, g_config, static_cast<int>(len));
    }
    result.rx_rc = RadioSetRx(kRadio);

    waitms(static_cast<int>(kRetuneSettleMs));

    g_last_tune = result;
    return result;
}

/// Retunes to the selected fox and clears any now-stale signal history.
void retune_current() {
    const TuneResult result = tune(kFoxes[g_app.fox_index].freq_hz, g_app.gain_step, g_app.bw_index);
    g_app.tuned_ok = result.built;
    g_app.meter.reset();
    ui::set_hunt_tune_status(result.built, result.cfg_rc, result.rx_rc);
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

/// Starts a sweep of all 70 cm fox channels.
///
/// Worth running whenever you lose the trail: it answers "am I even chasing the
/// right fox?" before you spend twenty minutes walking the wrong bearing.
void begin_band_scan() {
    for (int i = 0; i < kFoxCount; ++i) {
        g_app.scan_results[i] = -200;
        g_app.scan_measured[i] = false;
        ui::update_scan_row(i, 0, false, -1);
    }
    g_app.scan_index = 0;
    g_app.scan_sample = 0;
    g_app.scan_peak = -200;
    g_app.scan_running = true;
}

/// Index of the strongest channel measured so far, or -1 if none.
int best_scanned() {
    int best = -1;
    for (int i = 0; i < kFoxCount; ++i) {
        if (g_app.scan_measured[i] && (best < 0 || g_app.scan_results[i] > g_app.scan_results[best])) {
            best = i;
        }
    }
    return best;
}

/// Advances the sweep by one reading. Called once per main-loop pass.
///
/// One channel used to be measured in a nested loop that blocked for a second
/// and a half; spreading it over passes keeps the radio calls two frames from
/// main() and leaves buttons responsive while it runs.
void service_band_scan() {
    if (!g_app.scan_running) {
        return;
    }

    if (g_app.scan_index >= kFoxCount) {
        g_app.scan_running = false;
        const int best = best_scanned();
        for (int i = 0; i < kFoxCount; ++i) {
            ui::update_scan_row(i, g_app.scan_results[i], g_app.scan_measured[i], best);
        }
        // Leave the radio where the hunt expects it.
        retune_current();
        return;
    }

    // First pass on a channel: tune to it.
    if (g_app.scan_sample == 0) {
        g_app.scan_peak = -200;
        const TuneResult result =
            tune(kFoxes[g_app.scan_index].freq_hz, g_app.gain_step, g_app.bw_index);
        ui::set_scan_status(g_app.scan_index, result.cfg_rc, result.rx_rc);
        if (!result.built) {
            // Only a frequency this part cannot synthesise gets skipped; a
            // refused config still gets measured, so the reading can be
            // compared against the others.
            ui::update_scan_row(g_app.scan_index, 0, false, -1);
            ++g_app.scan_index;
            return;
        }
    }

    // Peak-hold rather than mean: a beacon keying on and off would otherwise
    // average down to nothing.
    const int dbm = read_rssi_dbm();
    if (dbm > g_app.scan_peak) {
        g_app.scan_peak = dbm;
    }
    ++g_app.scan_sample;

    if (g_app.scan_sample >= kScanSamplesPerChannel) {
        g_app.scan_results[g_app.scan_index] = g_app.scan_peak;
        g_app.scan_measured[g_app.scan_index] = true;
        ui::update_scan_row(g_app.scan_index, g_app.scan_peak, true, best_scanned());
        ++g_app.scan_index;
        g_app.scan_sample = 0;
    }
}

void enter_select() {
    g_app.screen = Screen::kSelect;
    // Not clearing the LEDs: they carry the per-button event flashes, which are
    // currently the only evidence of whether presses reach the app.
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
    ui::show_scan();
    begin_band_scan();
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
            g_app.pending = Request::kHunt;
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            g_app.pending = Request::kScan;
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
                g_app.pending = Request::kHunt;
            }
            break;
        case FWGUI_EVENT_YELLOW_BUTTON:
            if (g_app.gain_step < (cc1101::kGainSteps - 1)) {
                ++g_app.gain_step;
                g_app.pending = Request::kHunt;
            }
            break;
        case FWGUI_EVENT_GREEN_BUTTON:
            g_app.pending = Request::kRose;
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            g_app.pending = Request::kSelect;
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
            g_app.pending = Request::kRescan;
            break;
        case FWGUI_EVENT_BLUE_BUTTON:
            g_app.pending = Request::kSelect;
            break;
        case FWGUI_EVENT_RED_BUTTON:
            g_app.should_exit = true;
            break;
        default:
            break;
    }
}

/// Carries out the work a button asked for, at main-loop depth.
void service_request() {
    const Request request = g_app.pending;
    g_app.pending = Request::kNone;

    switch (request) {
        case Request::kSelect:
            enter_select();
            break;
        case Request::kHunt:
            enter_hunt();
            break;
        case Request::kRose:
            enter_rose();
            break;
        case Request::kScan:
            enter_scan();
            break;
        case Request::kRescan:
            begin_band_scan();
            break;
        case Request::kNone:
        default:
            break;
    }
}

void pump_events() {
    // Drain a bounded number of events per pass, never an open `while`.
    //
    // The unbounded version locked the board up hard. Nothing inside this loop
    // yields, so if hasEvent() keeps reporting true - because events arrive as
    // fast as they are drained, or because getEventData() does not consume the
    // one it returns - the app spins forever, the display processor is starved,
    // and the whole device stops responding. The cap means a busy queue costs a
    // few extra passes instead of the machine.
    constexpr int kMaxEventsPerPass = 8;

    uint8_t data[FW_GET_EVENT_DATA_MAX];
    for (int drained = 0; drained < kMaxEventsPerPass && hasEvent() != 0; ++drained) {
        const int event = getEventData(data);
        flash_event_led(event);

        // Surface anything outside the five button codes, with a count so a
        // repeating event is distinguishable from a one-off.
        if (event != FWGUI_EVENT_GRAY_BUTTON && event != FWGUI_EVENT_YELLOW_BUTTON &&
            event != FWGUI_EVENT_GREEN_BUTTON && event != FWGUI_EVENT_BLUE_BUTTON &&
            event != FWGUI_EVENT_RED_BUTTON) {
            ++g_app.odd_events;
            g_app.last_odd_event = event;
            ui::set_select_debug(event, g_app.odd_events);
        }
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

    // Put the receiver into RX once. With retuning disabled this is the only
    // radio state change the app makes, and RSSI needs the receiver open.
    RadioSetRx(kRadio);

    enter_select();

    while (!g_app.should_exit) {
        pump_events();
        if (g_app.should_exit) {
            break;
        }
        service_request();
        service_band_scan();
        service_screen();

        // Advance here rather than inside service_screen(), which returns early
        // on the screens that do not sample. Leaving it there pinned the
        // counter at zero on the select screen, so the heartbeat below fired on
        // every pass and flooded the LED queue twenty times a second.
        ++g_app.frame;

        // Heartbeat, so a running app is distinguishable from a frozen one.
        if ((g_app.frame % 20) == 0) {
            ui::board_led(6, 255, 0, 0, 400, ledpulsefade);
        }

        waitms(static_cast<int>(kSampleMs));
        g_app.now_ms += kSampleMs;
    }

    // Leave the hardware quiet on the way out.
    //
    // exitToMainAppMenu() is deliberately not called. The step ladder only ever
    // imported it, never invoked it, and calling it is what the app was doing
    // when it crashed on exit. Returning from main() ends the script regardless.
    RadioSetIdle(kRadio);
    ui::clear_leds();
    return 0;
}
