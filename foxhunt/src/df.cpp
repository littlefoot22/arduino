#include "df.h"

namespace foxhunt::df {
namespace {

/// EMA shift factors: the fast average settles in a few samples, the slow one
/// over a few seconds. Their difference is the warmer/colder signal.
constexpr int kFastShift = 2;
constexpr int kSlowShift = 5;

/// Trend has to clear this much (in dB) before it stops reporting flat, so that
/// receiver noise does not flicker the indicator while standing still.
constexpr int32_t kTrendDeadbandQ = 1 * kQ;

/// Peak and trough relax toward the current reading at this rate per sample,
/// which at ~20 samples/s works out to roughly 1 dB per second. Slow enough to
/// hold a peak through an antenna swing, quick enough to follow a real change.
constexpr int32_t kHoldDecayQ = kQ / 20;

/// Floor for the auto-ranged scale, so a dead-flat signal does not divide by
/// zero or amplify noise into a full-scale swing.
constexpr int32_t kMinSpanQ = 6 * kQ;

/// Nothing is ever this weak; used to mark an empty rose sector.
constexpr int16_t kNoReading = -200;

int clamp(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

/// Rounds a Q8 value back to a whole number, toward negative infinity for
/// negatives so that dBm readings do not drift upward.
int from_q(int32_t q) {
    if (q >= 0) {
        return static_cast<int>((q + (kQ / 2)) / kQ);
    }
    return -static_cast<int>(((-q) + (kQ / 2)) / kQ);
}

}  // namespace

const RoseOffset kRoseOffsets[kRoseSectors] = {
    {0, -1000},    {383, -924},  {707, -707},  {924, -383}, {1000, 0},    {924, 383},
    {707, 707},    {383, 924},   {0, 1000},    {-383, 924}, {-707, 707},  {-924, 383},
    {-1000, 0},    {-924, -383}, {-707, -707}, {-383, -924},
};

// ---------------------------------------------------------------- Meter -----

Meter::Meter() { reset(); }

void Meter::reset() {
    fast_ = 0;
    slow_ = 0;
    peak_ = 0;
    trough_ = 0;
    have_sample_ = false;
}

void Meter::push(int dbm) {
    const int32_t sample = static_cast<int32_t>(dbm) * kQ;

    if (!have_sample_) {
        fast_ = sample;
        slow_ = sample;
        peak_ = sample;
        trough_ = sample;
        have_sample_ = true;
        return;
    }

    fast_ += (sample - fast_) >> kFastShift;
    slow_ += (sample - slow_) >> kSlowShift;

    if (fast_ > peak_) {
        peak_ = fast_;
    } else {
        peak_ -= kHoldDecayQ;
        if (peak_ < fast_) {
            peak_ = fast_;
        }
    }

    if (fast_ < trough_) {
        trough_ = fast_;
    } else {
        trough_ += kHoldDecayQ;
        if (trough_ > fast_) {
            trough_ = fast_;
        }
    }
}

int Meter::smoothed() const { return from_q(fast_); }

int Meter::peak() const { return from_q(peak_); }

int Meter::trough() const { return from_q(trough_); }

Trend Meter::trend() const {
    if (!have_sample_) {
        return Trend::kFlat;
    }
    const int32_t delta = fast_ - slow_;
    if (delta > kTrendDeadbandQ) {
        return Trend::kWarmer;
    }
    if (delta < -kTrendDeadbandQ) {
        return Trend::kColder;
    }
    return Trend::kFlat;
}

int Meter::percent() const {
    if (!have_sample_) {
        return 0;
    }
    int32_t span = peak_ - trough_;
    if (span < kMinSpanQ) {
        span = kMinSpanQ;
    }
    const int32_t above = fast_ - trough_;
    const int32_t pct = (above * 100) / span;
    return clamp(static_cast<int>(pct), 0, 100);
}

// --------------------------------------------------------- RotationScan -----

RotationScan::RotationScan() : start_ms_(0U), duration_ms_(0U), active_(false), complete_(false) {
    for (int i = 0; i < kRoseSectors; ++i) {
        level_[i] = kNoReading;
        filled_[i] = false;
    }
}

void RotationScan::begin(uint32_t now_ms, uint32_t duration_ms) {
    for (int i = 0; i < kRoseSectors; ++i) {
        level_[i] = kNoReading;
        filled_[i] = false;
    }
    start_ms_ = now_ms;
    duration_ms_ = (duration_ms == 0U) ? 1U : duration_ms;
    active_ = true;
    complete_ = false;
}

void RotationScan::cancel() { active_ = false; }

int RotationScan::sector_for(uint32_t now_ms) const {
    const uint32_t elapsed = now_ms - start_ms_;
    if (elapsed >= duration_ms_) {
        return kRoseSectors - 1;
    }
    const uint64_t scaled = static_cast<uint64_t>(elapsed) * static_cast<uint64_t>(kRoseSectors);
    return static_cast<int>(scaled / duration_ms_);
}

void RotationScan::push(uint32_t now_ms, int dbm) {
    if (!active_) {
        return;
    }

    const int sector = clamp(sector_for(now_ms), 0, kRoseSectors - 1);
    const int16_t reading = static_cast<int16_t>(clamp(dbm, -160, 20));

    // Keep the peak per sector rather than the mean: the beacon may be keyed
    // intermittently, and a silent moment inside a sector should not be
    // averaged in as if the signal were genuinely weak there.
    if (!filled_[sector] || reading > level_[sector]) {
        level_[sector] = reading;
        filled_[sector] = true;
    }

    if ((now_ms - start_ms_) >= duration_ms_) {
        active_ = false;
        complete_ = true;
    }
}

int RotationScan::progress_pct(uint32_t now_ms) const {
    if (!active_) {
        return complete_ ? 100 : 0;
    }
    const uint32_t elapsed = now_ms - start_ms_;
    const uint64_t pct = (static_cast<uint64_t>(elapsed) * 100U) / duration_ms_;
    return clamp(static_cast<int>(pct), 0, 100);
}

bool RotationScan::sector_filled(int sector) const {
    if (sector < 0 || sector >= kRoseSectors) {
        return false;
    }
    return filled_[sector];
}

int RotationScan::sector_level(int sector) const {
    if (!sector_filled(sector)) {
        return weakest_db();
    }
    return static_cast<int>(level_[sector]);
}

int RotationScan::best_sector() const {
    int best = -1;
    int16_t best_level = kNoReading;
    for (int i = 0; i < kRoseSectors; ++i) {
        if (filled_[i] && (best < 0 || level_[i] > best_level)) {
            best = i;
            best_level = level_[i];
        }
    }
    return best;
}

int RotationScan::bearing_deg() const {
    const int sector = best_sector();
    if (sector < 0) {
        return -1;
    }
    return (sector * 360) / kRoseSectors;
}

int RotationScan::strongest_db() const {
    const int sector = best_sector();
    if (sector < 0) {
        return kNoReading;
    }
    return static_cast<int>(level_[sector]);
}

int RotationScan::weakest_db() const {
    int worst = 0;
    bool any = false;
    for (int i = 0; i < kRoseSectors; ++i) {
        if (filled_[i] && (!any || level_[i] < worst)) {
            worst = static_cast<int>(level_[i]);
            any = true;
        }
    }
    return any ? worst : kNoReading;
}

int RotationScan::contrast_db() const {
    if (best_sector() < 0) {
        return 0;
    }
    return strongest_db() - weakest_db();
}

// ------------------------------------------------------------- Feedback -----

int tone_hz_for(int percent) {
    const int pct = clamp(percent, 0, 100);
    // 400 Hz at nothing up to 2400 Hz at full scale: a range that stays audible
    // outdoors without becoming shrill over a long hunt.
    return 400 + ((pct * 2000) / 100);
}

int chirp_interval_ms_for(int percent) {
    const int pct = clamp(percent, 0, 100);
    // 700 ms when cold down to 90 ms when hot.
    return 700 - ((pct * 610) / 100);
}

}  // namespace foxhunt::df
