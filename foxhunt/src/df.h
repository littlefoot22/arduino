#pragma once

#include <cstdint>

/// Direction-finding maths. Like cc1101.h this is host-testable: no host calls,
/// no floating point, everything in Q8 fixed point so results are bit-identical
/// on the build machine and on the device.
namespace foxhunt::df {

/// Fixed-point scale. dBm values are small and negative, so Q8 in an int32
/// leaves enormous headroom.
inline constexpr int kQ = 256;

/// Sectors the rotation sweep is divided into: 16 gives 22.5 degrees each,
/// which is about the resolution a body-shielded whip can actually resolve.
inline constexpr int kRoseSectors = 16;

/// Returned by Meter::trend().
enum class Trend : int8_t { kColder = -1, kFlat = 0, kWarmer = 1 };

/// Smooths the noisy per-sample RSSI into something a needle can follow, and
/// works out whether the operator is walking towards the beacon or away.
///
/// Two exponential averages run at different rates: the fast one is what gets
/// displayed, and its position relative to the slow one is the trend. That
/// comparison is what makes "warmer/colder" work while walking, since absolute
/// dBm means little without knowing the beacon's power or the terrain.
class Meter {
public:
    Meter();

    /// Discards history. Call when changing fox, gain or bandwidth, since any
    /// of those makes previous samples incomparable.
    void reset();

    /// Feeds one RSSI reading in dBm.
    void push(int dbm);

    /// True once at least one sample has arrived.
    bool valid() const { return have_sample_; }

    /// Displayed signal level in dBm.
    int smoothed() const;

    /// Strongest reading seen recently. Decays slowly so that a peak found by
    /// swinging the antenna stays on screen long enough to act on.
    int peak() const;

    /// Weakest reading seen recently; the bottom of the auto-ranged scale.
    int trough() const;

    Trend trend() const;

    /// Signal as 0-100 across the recently observed range.
    ///
    /// The scale is relative, not absolute: as you close in, the window slides
    /// upward so the meter keeps resolving differences instead of sitting
    /// pinned at full scale. It answers "stronger than a moment ago?", which is
    /// the only question that matters while hunting.
    int percent() const;

private:
    int32_t fast_;
    int32_t slow_;
    int32_t peak_;
    int32_t trough_;
    bool have_sample_;
};

/// Captures a bearing by turning the operator into the antenna rotator.
///
/// The FreeWili has an accelerometer but no magnetometer, so there is no
/// magnetic heading to read and no gyro to integrate. What it can do is the
/// classic body-shielding sweep: hold the device against your chest, start the
/// scan, and turn steadily through one full circle. Your body attenuates the
/// signal arriving from behind you, so RSSI peaks when you are facing the fox.
///
/// Sectors are therefore binned by elapsed time, and the bearing that comes out
/// is relative to wherever you were pointing when you pressed start - not
/// magnetic north. Turning at a roughly even rate is what keeps it honest.
class RotationScan {
public:
    RotationScan();

    /// Clears the rose and starts a sweep lasting `duration_ms`.
    void begin(uint32_t now_ms, uint32_t duration_ms);

    /// Abandons a sweep in progress, keeping whatever was already collected.
    void cancel();

    bool active() const { return active_; }

    /// True once a sweep has completed and a bearing is available.
    bool has_result() const { return complete_; }

    /// Bins one reading. Ends the sweep automatically once the duration is up.
    void push(uint32_t now_ms, int dbm);

    /// Sweep progress, 0-100.
    int progress_pct(uint32_t now_ms) const;

    /// Peak dBm recorded in `sector`, or trough() if nothing landed there.
    int sector_level(int sector) const;

    /// True if any sample was binned into `sector`.
    bool sector_filled(int sector) const;

    /// Sector holding the strongest reading, or -1 if the rose is empty.
    int best_sector() const;

    /// Bearing to the fox in degrees clockwise from the start heading,
    /// or -1 if unknown.
    int bearing_deg() const;

    /// Spread between the strongest and weakest sector, in dB.
    ///
    /// A deep null means a trustworthy bearing. A flat rose means the signal is
    /// arriving from everywhere - reflections, or you are too close - and the
    /// bearing should not be believed.
    int contrast_db() const;

    int strongest_db() const;
    int weakest_db() const;

private:
    int sector_for(uint32_t now_ms) const;

    int16_t level_[kRoseSectors];
    bool filled_[kRoseSectors];
    uint32_t start_ms_;
    uint32_t duration_ms_;
    bool active_;
    bool complete_;
};

/// Maps a signal percentage to an audible tone in Hz.
///
/// Pitch rising with signal is what lets the hunt happen with the screen at
/// your side while you watch the ground you are walking over.
int tone_hz_for(int percent);

/// Gap between chirps in ms: faster as the signal climbs, Geiger-counter style.
int chirp_interval_ms_for(int percent);

/// Unit-circle offsets for each rose sector, scaled by 1000, sector 0 straight
/// up and increasing clockwise. Precomputed to keep libm out of the binary.
struct RoseOffset {
    int16_t dx;
    int16_t dy;
};

extern const RoseOffset kRoseOffsets[kRoseSectors];

}  // namespace foxhunt::df
