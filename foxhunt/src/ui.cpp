#include "ui.h"

#include <fwwasm.h>

#include "cc1101.h"
#include "foxes.h"

namespace foxhunt::ui {
namespace {

// ------------------------------------------------------------ formatting ----
//
// snprintf drags a large chunk of libc into a binary that has to fit in a
// single 64 KB WASM page, so these build strings by hand instead.

constexpr int kBufLen = 48;

/// Fixed-capacity string builder. Silently stops at capacity rather than
/// overflowing.
class Buf {
public:
    Buf() : len_(0) { data_[0] = '\0'; }

    void clear() {
        len_ = 0;
        data_[0] = '\0';
    }

    Buf& str(const char* text) {
        while (text != nullptr && *text != '\0' && len_ < (kBufLen - 1)) {
            data_[len_] = *text;
            ++len_;
            ++text;
        }
        data_[len_] = '\0';
        return *this;
    }

    Buf& ch(char c) {
        if (len_ < (kBufLen - 1)) {
            data_[len_] = c;
            ++len_;
            data_[len_] = '\0';
        }
        return *this;
    }

    /// Appends `value`, zero-padded to at least `pad` digits.
    Buf& num(int value, int pad = 0) {
        if (value < 0) {
            ch('-');
            value = -value;
        }
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
        return *this;
    }

    /// Appends a frequency as MHz with three decimals, e.g. "446.025".
    Buf& mhz(uint32_t hz) {
        num(static_cast<int>(hz / 1000000U));
        ch('.');
        num(static_cast<int>((hz % 1000000U) / 1000U), 3);
        return *this;
    }

    const char* c_str() const { return data_; }

private:
    char data_[kBufLen];
    int len_;
};

// ---------------------------------------------------------------- colors ----

struct Rgb {
    int r;
    int g;
    int b;
};

constexpr Rgb kWhite{255, 255, 255};
constexpr Rgb kDim{140, 150, 160};
constexpr Rgb kAmber{255, 176, 0};
constexpr Rgb kGreen{0, 230, 90};
constexpr Rgb kBg{8, 12, 18};

// Font type: 0 is monospace, 1 proportional.
constexpr int kMono = 0;
constexpr int kProp = 1;

// The two "font size" arguments in this API are not the same unit, which is
// easy to get wrong because the parameter has the same name in both:
//
//   addControlText()   - size in PIXELS       (vendor radio example passes 64)
//   addControlNumber() - size as a 0-3 INDEX  (vendor ball example passes 3)
//
// Passing an index to addControlText() renders text a couple of pixels tall,
// which on a dark background looks exactly like a screen that failed to draw.
constexpr int kTextHuge = 44;
constexpr int kTextTitle = 26;
constexpr int kTextRow = 20;
constexpr int kTextLabel = 16;
constexpr int kTextSmall = 13;

/// Font size index for addControlNumber(), which does take an index.
constexpr int kNumberFont = 3;

// -------------------------------------------------------- control indices ---

// Control indices are kept low, contiguous and few.
//
// The first version of this file numbered them sparsely, with the band scan
// reaching index 36 and the rose holding twenty controls on one panel. The app
// did not draw. Every vendor example stays under ten, so a fixed-size per-panel
// array in the firmware is the likely explanation. Nothing below exceeds
// twelve controls on a panel or an index of eleven.

enum SelectControl : int {
    kSelTitle = 0,
    kSelFirstRow = 1,  // five rows follow: 1-5
    kSelNote = 6,
};

enum HuntControl : int {
    kHuntFox = 0,
    kHuntFreq = 1,
    kHuntDbm = 2,
    kHuntBar = 3,
    kHuntPeak = 4,
    kHuntTrend = 5,
    kHuntGain = 6,
    kHuntStatus = 7,
};

enum RoseControl : int {
    kRoseTitle = 0,
    kRoseBearing = 1,
    kRoseDetail = 2,
    kRoseHint = 3,
    kRoseFirstDot = 4,  // eight dots follow: 4-11
};

enum ScanControl : int {
    kScanTitle = 0,
    // One text control per row carries the fox name and its reading together,
    // rather than two, which halves the control count on this panel.
    kScanFirstRow = 1,  // five rows: 1-5
    kScanFirstBar = 6,  // five bargraphs: 6-10
};

/// Center and radius of the compass rose.
constexpr int kRoseCx = 158;
constexpr int kRoseCy = 118;
constexpr int kRoseRadius = 74;

Buf g_buf;

void set_text(int panel, int control, const char* text) { setControlValueText(panel, control, text); }

/// Renders one rose sector as a glyph whose weight tracks signal strength.
///
/// The host API's colour and reposition properties take packed integers whose
/// layout is not documented, so the rose is drawn by swapping characters -
/// which uses only the plainly specified setControlValueText().
const char* rose_glyph(int level_db, int weakest, int strongest, bool filled, bool is_peak) {
    if (!filled) {
        return ".";  // nothing sampled here
    }
    if (is_peak) {
        return "@";
    }
    const int span = (strongest - weakest) < 1 ? 1 : (strongest - weakest);
    const int pct = ((level_db - weakest) * 100) / span;
    if (pct >= 75) {
        return "O";
    }
    if (pct >= 45) {
        return "o";
    }
    if (pct >= 20) {
        return "+";
    }
    return "-";
}

/// Lights a build-stage marker.
///
/// build_all() is where the app stops on this firmware, and the screen cannot
/// report that, so each panel signs off on an LED as it completes.
void mark(int led, int r, int g, int b) { setBoardLED(led, r, g, b, 60000, ledsimplevalue); }

}  // namespace

// ------------------------------------------------------------ construction --

void build_all() {
    // ---- fox select ----
    addPanel(kPanelSelect, 1, 0, 0, 0, kBg.r, kBg.g, kBg.b, 1);
    addControlText(kPanelSelect, kSelTitle, 8, 4, kProp, kTextTitle, kAmber.r, kAmber.g, kAmber.b,
                   "FOXHUNT  70cm");
    for (int i = 0; i < kFoxCount; ++i) {
        addControlText(kPanelSelect, kSelFirstRow + i, 14, 44 + (i * 28), kMono, kTextRow, kWhite.r,
                       kWhite.g, kWhite.b, " ");
    }
    addControlText(kPanelSelect, kSelNote, 8, 200, kProp, kTextSmall, kDim.r, kDim.g, kDim.b,
                   "Fox 1-5 are 2m - outside CC1101 range");
    setPanelMenuText(kPanelSelect, 0, "UP");
    setPanelMenuText(kPanelSelect, 1, "DOWN");
    setPanelMenuText(kPanelSelect, 2, "HUNT");
    setPanelMenuText(kPanelSelect, 3, "SCAN");
    setPanelMenuText(kPanelSelect, 4, "EXIT");

    mark(1, 0, 0, 255);

    // ---- hunt meter ----
    addPanel(kPanelHunt, 1, 0, 0, 0, kBg.r, kBg.g, kBg.b, 1);
    addControlText(kPanelHunt, kHuntFox, 8, 4, kProp, kTextTitle, kAmber.r, kAmber.g, kAmber.b,
                   "FOX");
    addControlText(kPanelHunt, kHuntFreq, 150, 10, kMono, kTextLabel, kDim.r, kDim.g, kDim.b, " ");
    addControlNumber(kPanelHunt, kHuntDbm, 1, 14, 36, 200, kNumberFont, kMono, kWhite.r, kWhite.g,
                     kWhite.b, 0, 0, 0, 0);
    addControlBargraph(kPanelHunt, kHuntBar, 1, 14, 96, 292, 34, 0, 100, kGreen.r, kGreen.g,
                       kGreen.b);
    addControlText(kPanelHunt, kHuntPeak, 14, 138, kMono, kTextLabel, kDim.r, kDim.g, kDim.b, " ");
    addControlText(kPanelHunt, kHuntTrend, 14, 160, kProp, kTextHuge, kWhite.r, kWhite.g, kWhite.b,
                   " ");
    addControlText(kPanelHunt, kHuntGain, 14, 206, kMono, kTextSmall, kDim.r, kDim.g, kDim.b, " ");
    addControlText(kPanelHunt, kHuntStatus, 14, 222, kProp, kTextSmall, kDim.r, kDim.g, kDim.b, " ");
    setPanelMenuText(kPanelHunt, 0, "ATT-");
    setPanelMenuText(kPanelHunt, 1, "ATT+");
    setPanelMenuText(kPanelHunt, 2, "ROSE");
    setPanelMenuText(kPanelHunt, 3, "BACK");
    setPanelMenuText(kPanelHunt, 4, "EXIT");

    mark(2, 0, 255, 0);

    // ---- rotation rose ----
    addPanel(kPanelRose, 1, 0, 0, 0, kBg.r, kBg.g, kBg.b, 1);
    addControlText(kPanelRose, kRoseTitle, 8, 4, kProp, kTextLabel, kAmber.r, kAmber.g, kAmber.b,
                   "ROTATION SCAN");
    for (int i = 0; i < df::kRoseSectors; ++i) {
        const int x = kRoseCx + ((df::kRoseOffsets[i].dx * kRoseRadius) / 1000);
        const int y = kRoseCy + ((df::kRoseOffsets[i].dy * kRoseRadius) / 1000);
        addControlText(kPanelRose, kRoseFirstDot + i, x, y, kMono, kTextRow, kDim.r, kDim.g, kDim.b,
                       "*");
    }
    addControlText(kPanelRose, kRoseBearing, kRoseCx - 34, kRoseCy - 14, kProp, kTextTitle,
                   kWhite.r, kWhite.g, kWhite.b, "--");
    addControlText(kPanelRose, kRoseDetail, 8, 200, kMono, kTextSmall, kDim.r, kDim.g, kDim.b, " ");
    addControlText(kPanelRose, kRoseHint, 8, 218, kProp, kTextSmall, kDim.r, kDim.g, kDim.b,
                   "Hold to chest, turn a full circle");
    setPanelMenuText(kPanelRose, 0, " ");
    setPanelMenuText(kPanelRose, 1, " ");
    setPanelMenuText(kPanelRose, 2, "START");
    setPanelMenuText(kPanelRose, 3, "BACK");
    setPanelMenuText(kPanelRose, 4, "EXIT");

    mark(3, 255, 255, 0);

    // ---- band scan ----
    addPanel(kPanelScan, 1, 0, 0, 0, kBg.r, kBg.g, kBg.b, 1);
    addControlText(kPanelScan, kScanTitle, 8, 4, kProp, kTextTitle, kAmber.r, kAmber.g, kAmber.b,
                   "BAND SCAN");
    for (int i = 0; i < kFoxCount; ++i) {
        const int y = 44 + (i * 34);
        addControlText(kPanelScan, kScanFirstRow + i, 8, y, kMono, kTextRow, kWhite.r, kWhite.g,
                       kWhite.b, kFoxes[i].name);
        addControlBargraph(kPanelScan, kScanFirstBar + i, 1, 150, y, 160, 20, 0, 100, kGreen.r,
                           kGreen.g, kGreen.b);
    }
    setPanelMenuText(kPanelScan, 0, " ");
    setPanelMenuText(kPanelScan, 1, " ");
    setPanelMenuText(kPanelScan, 2, "RESCAN");
    setPanelMenuText(kPanelScan, 3, "BACK");
    setPanelMenuText(kPanelScan, 4, "EXIT");

    mark(4, 255, 0, 255);
}

void show_select(int selected_index) {
    update_select(selected_index);
    showPanel(kPanelSelect);
}

void show_hunt() { showPanel(kPanelHunt); }

void show_rose() { showPanel(kPanelRose); }

void show_scan() { showPanel(kPanelScan); }

// ----------------------------------------------------------------- update ---

void update_select(int selected_index) {
    for (int i = 0; i < kFoxCount; ++i) {
        g_buf.clear();
        g_buf.str(i == selected_index ? "> " : "  ");
        g_buf.str(kFoxes[i].name);
        g_buf.str("   ");
        g_buf.mhz(kFoxes[i].freq_hz);
        set_text(kPanelSelect, kSelFirstRow + i, g_buf.c_str());
    }
}

void update_hunt(int fox_index, const df::Meter& meter, int gain_step, int bw_index, bool tunable) {
    const Fox& fox = kFoxes[fox_index];

    set_text(kPanelHunt, kHuntFox, fox.name);

    g_buf.clear();
    g_buf.mhz(fox.freq_hz).str(" MHz");
    set_text(kPanelHunt, kHuntFreq, g_buf.c_str());

    if (!tunable) {
        setControlValue(kPanelHunt, kHuntDbm, 0);
        setControlValue(kPanelHunt, kHuntBar, 0);
        set_text(kPanelHunt, kHuntTrend, "OUT OF BAND");
        set_text(kPanelHunt, kHuntPeak, " ");
        return;
    }

    const int percent = meter.percent();

    setControlValue(kPanelHunt, kHuntDbm, meter.valid() ? meter.smoothed() : 0);
    setControlValue(kPanelHunt, kHuntBar, percent);

    g_buf.clear();
    if (meter.valid()) {
        g_buf.str("dBm   peak ").num(meter.peak()).str("  low ").num(meter.trough());
    } else {
        g_buf.str("waiting for receiver");
    }
    set_text(kPanelHunt, kHuntPeak, g_buf.c_str());

    switch (meter.trend()) {
        case df::Trend::kWarmer:
            set_text(kPanelHunt, kHuntTrend, ">>> WARMER");
            break;
        case df::Trend::kColder:
            set_text(kPanelHunt, kHuntTrend, "<<< colder");
            break;
        case df::Trend::kFlat:
        default:
            set_text(kPanelHunt, kHuntTrend, "--- steady");
            break;
    }

    g_buf.clear();
    g_buf.str("ATT ").num(gain_step).ch('/').num(cc1101::kGainSteps - 1);
    g_buf.str("   BW ").num(static_cast<int>(cc1101::kBwKhz[bw_index])).str(" kHz");
    set_text(kPanelHunt, kHuntGain, g_buf.c_str());
}

void set_hunt_status(const char* text) { set_text(kPanelHunt, kHuntStatus, text); }

void update_rose(const df::RotationScan& scan, uint32_t now_ms) {
    const int best = scan.best_sector();
    const int strongest = scan.strongest_db();
    const int weakest = scan.weakest_db();

    for (int i = 0; i < df::kRoseSectors; ++i) {
        set_text(kPanelRose, kRoseFirstDot + i,
                 rose_glyph(scan.sector_level(i), weakest, strongest, scan.sector_filled(i),
                            i == best));
    }

    g_buf.clear();
    if (scan.active()) {
        g_buf.num(scan.progress_pct(now_ms)).ch('%');
        set_text(kPanelRose, kRoseBearing, g_buf.c_str());
        set_text(kPanelRose, kRoseDetail, "turning...");
        return;
    }

    if (!scan.has_result() || best < 0) {
        set_text(kPanelRose, kRoseBearing, "--");
        set_text(kPanelRose, kRoseDetail, "press START, then turn");
        return;
    }

    g_buf.num(scan.bearing_deg()).str(" deg");
    set_text(kPanelRose, kRoseBearing, g_buf.c_str());

    // A rose with little variation means the bearing is not trustworthy: the
    // signal is arriving off reflections, or you are close enough that body
    // shielding no longer produces a null.
    const int contrast = scan.contrast_db();
    g_buf.clear();
    g_buf.str("peak ").num(strongest).str(" dBm  null depth ").num(contrast).str(" dB");
    set_text(kPanelRose, kRoseDetail, g_buf.c_str());
    set_text(kPanelRose, kRoseHint,
             contrast >= 6 ? "Good null - walk that way"
                           : "Weak null - move and scan again");
}

void update_scan_row(int fox_index, int dbm, bool measured, int strongest_index) {
    // Name, marker and reading share one control.
    g_buf.clear();
    g_buf.str(fox_index == strongest_index ? "*" : " ").str(kFoxes[fox_index].name).ch(' ');
    if (measured) {
        g_buf.num(dbm);
    } else {
        g_buf.str("--");
    }
    set_text(kPanelScan, kScanFirstRow + fox_index, g_buf.c_str());

    // -110 dBm is about the CC1101's noise floor and -40 a very close beacon;
    // that span maps the whole approach onto the bar.
    int pct = 0;
    if (measured) {
        pct = ((dbm + 110) * 100) / 70;
        pct = (pct < 0) ? 0 : ((pct > 100) ? 100 : pct);
    }
    setControlValue(kPanelScan, kScanFirstBar + fox_index, pct);
}

void update_leds(int percent) {
    // Light LEDs proportionally, green through amber to red as signal climbs,
    // so the board reads as a meter from the corner of your eye.
    const int lit = (percent * kBoardLeds) / 100;
    for (int i = 0; i < kBoardLeds; ++i) {
        if (i < lit) {
            const int ramp = (i * 255) / (kBoardLeds - 1);
            setBoardLED(i, ramp, 255 - ramp, 0, 200, ledsimplevalue);
        } else {
            setBoardLED(i, 0, 0, 0, 200, ledsimplevalue);
        }
    }
}

void clear_leds() {
    for (int i = 0; i < kBoardLeds; ++i) {
        setBoardLED(i, 0, 0, 0, 100, ledsimplevalue);
    }
}

}  // namespace foxhunt::ui
