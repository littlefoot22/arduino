/// Host-side tests for the parts of the app that are pure arithmetic.
///
/// The radio maths and the direction-finding filters carry all the logic worth
/// getting wrong, and none of them touch the FreeWili host API - so they can be
/// checked on the build machine instead of by walking around a field.

#include <cstdio>

#include "cc1101.h"
#include "df.h"
#include "foxes.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const char* what, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

void test_band_limits() {
    using namespace foxhunt;

    // The whole reason the 2 m half of the contest is out of scope.
    CHECK(!cc1101::is_tunable(147420000U));
    CHECK(!cc1101::is_tunable(147545000U));

    // Every 70 cm fox must be reachable.
    for (int i = 0; i < kFoxCount; ++i) {
        CHECK(cc1101::is_tunable(kFoxes[i].freq_hz));
    }

    // Band edges.
    CHECK(cc1101::is_tunable(387000000U));
    CHECK(cc1101::is_tunable(464000000U));
    CHECK(!cc1101::is_tunable(386999999U));
    CHECK(!cc1101::is_tunable(464000001U));
}

void test_frequency_word() {
    using namespace foxhunt;

    // A round trip must land within one tuning step (~397 Hz at 26 MHz).
    const uint32_t step = cc1101::tuning_step_hz();
    CHECK(step > 380U && step < 420U);

    for (int i = 0; i < kFoxCount; ++i) {
        const uint32_t hz = kFoxes[i].freq_hz;
        const uint32_t word = cc1101::freq_to_word(hz);
        const uint32_t back = cc1101::word_to_freq(word);
        const uint32_t error = (back > hz) ? (back - hz) : (hz - back);
        CHECK(error <= step);
        CHECK(word <= 0xFFFFFFU);
    }

    // Distinct channels must produce distinct words, otherwise the 25 kHz
    // contest spacing would collapse and every fox would tune the same place.
    for (int i = 1; i < kFoxCount; ++i) {
        CHECK(cc1101::freq_to_word(kFoxes[i].freq_hz) !=
              cc1101::freq_to_word(kFoxes[i - 1].freq_hz));
    }

    // Cross-check against TI's published 433.92 MHz register set, where
    // FREQ2:FREQ1:FREQ0 = 0x10:0xB0:0x71. Anything else means the crystal
    // constant or the rounding is wrong.
    CHECK(cc1101::freq_to_word(433920000U) == 0x10B071U);
}

void test_config_blob() {
    using namespace foxhunt;

    uint8_t buf[cc1101::kConfigBytes];

    // Out-of-band frequencies must be refused rather than silently mistuned.
    CHECK(cc1101::build_rx_config(buf, sizeof(buf), 147420000U, 0, 0) == 0U);

    // Undersized buffers must be refused.
    CHECK(cc1101::build_rx_config(buf, 4U, 446025000U, 0, 0) == 0U);

    const size_t len = cc1101::build_rx_config(buf, sizeof(buf), 446025000U, 0, 0);
    CHECK(len == cc1101::kConfigBytes);

    // The blob is address/value pairs; find FREQ2/1/0 and confirm they carry
    // the word the frequency maths produced.
    const uint32_t word = cc1101::freq_to_word(446025000U);
    int found = 0;
    for (size_t i = 0; i < len; i += 2U) {
        const uint8_t reg = buf[i];
        const uint8_t val = buf[i + 1U];
        if (reg == 0x0D) {
            CHECK(val == static_cast<uint8_t>((word >> 16) & 0xFF));
            ++found;
        } else if (reg == 0x0E) {
            CHECK(val == static_cast<uint8_t>((word >> 8) & 0xFF));
            ++found;
        } else if (reg == 0x0F) {
            CHECK(val == static_cast<uint8_t>(word & 0xFF));
            ++found;
        }
    }
    CHECK(found == 3);

    // Sync-word detection must stay off or the receiver would gate RSSI behind
    // a packet that an analogue FM beacon never sends.
    for (size_t i = 0; i < len; i += 2U) {
        if (buf[i] == 0x12) {
            CHECK((buf[i + 1U] & 0x07U) == 0U);
        }
    }

    // Raising the attenuator step must actually change AGCCTRL2.
    uint8_t quiet[cc1101::kConfigBytes];
    cc1101::build_rx_config(quiet, sizeof(quiet), 446025000U, cc1101::kGainSteps - 1, 0);
    bool agc_differs = false;
    for (size_t i = 0; i < len; i += 2U) {
        if (buf[i] == 0x1B && quiet[i] == 0x1B && buf[i + 1U] != quiet[i + 1U]) {
            agc_differs = true;
        }
    }
    CHECK(agc_differs);

    // Bandwidth index must change MDMCFG4's upper nibble but leave the data
    // rate exponent in the low nibble alone.
    uint8_t wide[cc1101::kConfigBytes];
    cc1101::build_rx_config(wide, sizeof(wide), 446025000U, 0, cc1101::kBwSteps - 1);
    for (size_t i = 0; i < len; i += 2U) {
        if (buf[i] == 0x10) {
            CHECK((buf[i + 1U] & 0xF0U) != (wide[i + 1U] & 0xF0U));
            CHECK((buf[i + 1U] & 0x0FU) == (wide[i + 1U] & 0x0FU));
        }
    }

    // Out-of-range indices must clamp, not read out of bounds.
    CHECK(cc1101::build_rx_config(buf, sizeof(buf), 446025000U, 99, 99) == cc1101::kConfigBytes);
    CHECK(cc1101::build_rx_config(buf, sizeof(buf), 446025000U, -5, -5) == cc1101::kConfigBytes);
}

void test_rssi_normalisation() {
    using namespace foxhunt;

    // Already-dBm values pass through.
    CHECK(cc1101::normalize_rssi(-92) == -92);
    CHECK(cc1101::normalize_rssi(-40) == -40);

    // Raw register values convert per the datasheet: >=128 is negative,
    // half-dB steps, minus the 74 dB offset.
    CHECK(cc1101::normalize_rssi(0x80) == ((-128 / 2) - 74));  // -138
    CHECK(cc1101::normalize_rssi(0x7F) == ((127 / 2) - 74));    // -11

    // Conversion must stay monotonic across the register wrap.
    CHECK(cc1101::normalize_rssi(200) < cc1101::normalize_rssi(100));
}

void test_meter() {
    using namespace foxhunt;

    df::Meter meter;
    CHECK(!meter.valid());
    CHECK(meter.percent() == 0);

    meter.push(-100);
    CHECK(meter.valid());
    CHECK(meter.smoothed() == -100);

    // A steady signal reads flat, not drifting.
    for (int i = 0; i < 200; ++i) {
        meter.push(-100);
    }
    CHECK(meter.trend() == df::Trend::kFlat);
    CHECK(meter.smoothed() == -100);

    // Walking toward the fox: rising signal must read warmer.
    for (int i = 0; i < 40; ++i) {
        meter.push(-100 + i);
    }
    CHECK(meter.trend() == df::Trend::kWarmer);

    // And away again.
    df::Meter cooling;
    for (int i = 0; i < 100; ++i) {
        cooling.push(-50);
    }
    for (int i = 0; i < 40; ++i) {
        cooling.push(-50 - i);
    }
    CHECK(cooling.trend() == df::Trend::kColder);

    // Percentage must stay in bounds however wild the input.
    df::Meter noisy;
    for (int i = 0; i < 500; ++i) {
        noisy.push(((i % 7) * 13) - 120);
        CHECK(noisy.percent() >= 0 && noisy.percent() <= 100);
    }

    // Reset must forget everything.
    noisy.reset();
    CHECK(!noisy.valid());
    CHECK(noisy.percent() == 0);

    // Peak must sit at or above the trough, always.
    df::Meter held;
    for (int i = 0; i < 100; ++i) {
        held.push(-90 + (i % 20));
        CHECK(held.peak() >= held.trough());
    }
}

void test_level_scale() {
    using namespace foxhunt;

    // Ends of the scale, and the clamping beyond them.
    CHECK(df::level_percent(df::kFloorDbm) == 0);
    CHECK(df::level_percent(df::kCeilDbm) == 100);
    CHECK(df::level_percent(df::kFloorDbm - 40) == 0);
    CHECK(df::level_percent(df::kCeilDbm + 40) == 100);

    // Monotonic across the whole span.
    for (int dbm = df::kFloorDbm; dbm < df::kCeilDbm; ++dbm) {
        CHECK(df::level_percent(dbm) <= df::level_percent(dbm + 1));
    }

    // The bug this replaced: an auto-ranged bar reads zero on a steady signal,
    // however strong it is. A fixed scale must not.
    df::Meter steady;
    for (int i = 0; i < 300; ++i) {
        steady.push(-89);
    }
    CHECK(steady.percent() == 0);       // relative: correctly says "no change"
    CHECK(steady.level() > 20);         // absolute: says "there is a signal"
    CHECK(steady.level() == df::level_percent(-89));

    // A stronger steady signal must read higher than a weaker one, which is the
    // property the relative scale cannot offer.
    df::Meter strong;
    for (int i = 0; i < 300; ++i) {
        strong.push(-55);
    }
    CHECK(strong.level() > steady.level());

    // Nothing sampled yet reads zero rather than the floor's percentage.
    df::Meter fresh;
    CHECK(fresh.level() == 0);
}

void test_rotation_scan() {
    using namespace foxhunt;

    df::RotationScan scan;
    CHECK(!scan.active());
    CHECK(!scan.has_result());
    CHECK(scan.best_sector() == -1);
    CHECK(scan.bearing_deg() == -1);

    constexpr uint32_t kDuration = 16000U;
    scan.begin(0U, kDuration);
    CHECK(scan.active());

    // Simulate a turn where the beacon is strongest a quarter of the way round.
    constexpr int kPeakSector = df::kRoseSectors / 4;
    for (uint32_t t = 0U; t <= kDuration; t += 100U) {
        const int sector =
            static_cast<int>((t * static_cast<uint32_t>(df::kRoseSectors)) / kDuration);
        int offset = sector - kPeakSector;
        if (offset < 0) {
            offset = -offset;
        }
        if (offset > (df::kRoseSectors / 2)) {
            offset = df::kRoseSectors - offset;
        }
        scan.push(t, -70 - (offset * 3));
    }

    CHECK(!scan.active());
    CHECK(scan.has_result());
    CHECK(scan.best_sector() == kPeakSector);
    CHECK(scan.bearing_deg() == (kPeakSector * 360) / df::kRoseSectors);
    CHECK(scan.progress_pct(kDuration) == 100);

    // A pronounced peak must show real contrast.
    CHECK(scan.contrast_db() >= 6);
    CHECK(scan.strongest_db() > scan.weakest_db());

    // Every sector should have been visited by a full sweep.
    for (int i = 0; i < df::kRoseSectors; ++i) {
        CHECK(scan.sector_filled(i));
    }

    // A flat rose must report negligible contrast so the UI can warn instead of
    // pointing confidently at noise.
    df::RotationScan flat;
    flat.begin(0U, kDuration);
    for (uint32_t t = 0U; t <= kDuration; t += 100U) {
        flat.push(t, -80);
    }
    CHECK(flat.contrast_db() == 0);

    // Cancelling mid-sweep must stop it without claiming a result.
    df::RotationScan aborted;
    aborted.begin(0U, kDuration);
    aborted.push(1000U, -60);
    aborted.cancel();
    CHECK(!aborted.active());
    CHECK(!aborted.has_result());

    // Samples after cancellation are ignored.
    const int before = aborted.best_sector();
    aborted.push(2000U, 0);
    CHECK(aborted.best_sector() == before);

    // Out-of-range sector queries must not read past the array.
    CHECK(!scan.sector_filled(-1));
    CHECK(!scan.sector_filled(df::kRoseSectors));
    CHECK(!scan.sector_filled(9999));
}

void test_rose_geometry() {
    using namespace foxhunt;

    // Sector 0 points straight up, then a quarter turn clockwise each time:
    // right, down, left. Getting this wrong rotates every bearing by a quadrant.
    constexpr int q = df::kRoseSectors / 4;
    CHECK(df::kRoseOffsets[0].dx == 0 && df::kRoseOffsets[0].dy == -1000);
    CHECK(df::kRoseOffsets[q].dx == 1000 && df::kRoseOffsets[q].dy == 0);
    CHECK(df::kRoseOffsets[2 * q].dx == 0 && df::kRoseOffsets[2 * q].dy == 1000);
    CHECK(df::kRoseOffsets[3 * q].dx == -1000 && df::kRoseOffsets[3 * q].dy == 0);

    // Every offset must sit on the unit circle, within rounding.
    for (int i = 0; i < df::kRoseSectors; ++i) {
        const int dx = df::kRoseOffsets[i].dx;
        const int dy = df::kRoseOffsets[i].dy;
        const int r2 = (dx * dx) + (dy * dy);
        CHECK(r2 > 995 * 995 && r2 < 1005 * 1005);
    }
}

void test_audio_feedback() {
    using namespace foxhunt;

    // Pitch must rise with signal and stay inside an audible band.
    CHECK(df::tone_hz_for(0) < df::tone_hz_for(50));
    CHECK(df::tone_hz_for(50) < df::tone_hz_for(100));
    CHECK(df::tone_hz_for(0) >= 300);
    CHECK(df::tone_hz_for(100) <= 4000);

    // Chirps must get closer together as the signal grows, and never hit zero
    // (which would busy-loop the sound engine).
    CHECK(df::chirp_interval_ms_for(0) > df::chirp_interval_ms_for(100));
    CHECK(df::chirp_interval_ms_for(100) > 0);

    // Out-of-range percentages must clamp rather than extrapolate.
    CHECK(df::tone_hz_for(-50) == df::tone_hz_for(0));
    CHECK(df::tone_hz_for(500) == df::tone_hz_for(100));
    CHECK(df::chirp_interval_ms_for(-1) == df::chirp_interval_ms_for(0));
    CHECK(df::chirp_interval_ms_for(999) == df::chirp_interval_ms_for(100));
}

struct Test {
    const char* name;
    void (*fn)();
};

const Test kTests[] = {
    {"band limits", test_band_limits},
    {"frequency word", test_frequency_word},
    {"config blob", test_config_blob},
    {"rssi normalisation", test_rssi_normalisation},
    {"meter", test_meter},
    {"level scale", test_level_scale},
    {"rotation scan", test_rotation_scan},
    {"rose geometry", test_rose_geometry},
    {"audio feedback", test_audio_feedback},
};

}  // namespace

int main() {
    for (const Test& test : kTests) {
        const int before = g_failures;
        test.fn();
        std::printf("%-22s %s\n", test.name, (g_failures == before) ? "ok" : "FAILED");
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
