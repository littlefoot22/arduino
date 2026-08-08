#pragma once

#include <cstdint>

#include "df.h"

/// Panel construction and per-frame updates. Everything here talks to the
/// FreeWili host API, so this is the one module that cannot run on the desktop.
namespace foxhunt::ui {

/// Panel 0 is reserved by the firmware for the event log.
enum Panel : int {
    kPanelSelect = 1,
    kPanelHunt = 2,
    kPanelRose = 3,
    kPanelScan = 4,
};

/// Display geometry, confirmed against the stock example apps.
inline constexpr int kScreenW = 320;
inline constexpr int kScreenH = 240;

/// Board LEDs usable as a coarse signal bar.
inline constexpr int kBoardLeds = 7;

/// Percentage of full brightness the board LEDs are driven at.
///
/// These sit inches from your face in the dark, and at full scale they are
/// genuinely unpleasant to hunt with. Low enough to read at arm's length,
/// dim enough not to wreck your night vision.
inline constexpr int kLedBrightnessPct = 20;

/// Sets a board LED, scaled to kLedBrightnessPct. Use instead of setBoardLED.
void board_led(int index, int r, int g, int b, int duration_ms, int mode);

/// Builds every panel. Call once at startup.
void build_all();

void show_select(int selected_index);
void show_hunt();
void show_rose();
void show_scan();

/// Repaints the fox list with `selected_index` marked.
void update_select(int selected_index);

/// Repaints the hunt meter.
void update_hunt(int fox_index, const df::Meter& meter, int gain_step, int bw_index, bool tunable);

/// Repaints the rose. `now_ms` drives the progress readout during a sweep.
void update_rose(const df::RotationScan& scan, uint32_t now_ms);

/// Repaints one row of the band-scan table.
void update_scan_row(int fox_index, int dbm, bool measured, int strongest_index);

/// Drives the board LEDs as a 7-segment signal bar.
void update_leds(int percent);

/// Turns every board LED off.
void clear_leds();

/// Writes `text` into a message line on the hunt panel.
void set_hunt_status(const char* text);

/// Reports what the radio said about the last retune, on the hunt panel.
void set_hunt_tune_status(bool built, int cfg_rc, int rx_rc);

/// Reports scan progress and the radio's return codes, on the scan panel.
void set_scan_status(int channel, int cfg_rc, int rx_rc);

/// Shows the most recent unrecognised event code on the select panel.
///
/// The firmware sends events this app does not have names for, and the header
/// documents a newer API than the board implements, so the numbering cannot be
/// trusted to match. Printing the raw code is the only way to find out what
/// they actually are.
void set_select_debug(int last_event, int count);

}  // namespace foxhunt::ui
