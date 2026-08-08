#pragma once

#include <cstddef>
#include <cstdint>

/// Pure CC1101 register arithmetic. Nothing here calls into the WASM host, so
/// the whole file compiles and runs on the build machine - see tests/.
namespace foxhunt::cc1101 {

/// Crystal fitted to the FreeWili's CC1101s. Every frequency, bandwidth and
/// data-rate figure in the datasheet is derived from it.
inline constexpr uint32_t kXoscHz = 26000000U;

/// Tuning limits of the three synthesiser bands, used to reject frequencies the
/// part physically cannot reach.
struct Band {
    uint32_t low_hz;
    uint32_t high_hz;
};

inline constexpr Band kBands[] = {
    {300000000U, 348000000U},
    {387000000U, 464000000U},
    {779000000U, 928000000U},
};

inline constexpr int kBandCount = static_cast<int>(sizeof(kBands) / sizeof(kBands[0]));

/// Front-end gain steps, most sensitive first.
///
/// This is the software stand-in for the screw-on RF attenuator that is
/// standard foxhunting kit. Close to a beacon the AGC pins at full scale and
/// every direction reads equally strong; dropping gain restores the contrast
/// that makes the last hundred metres findable.
inline constexpr int kGainSteps = 5;

/// Receiver bandwidth choices, narrowest first. Narrow rejects the neighbouring
/// fox better; wide is more forgiving of the CC1101's crystal error against a
/// beacon that may itself be slightly off frequency.
inline constexpr int kBwSteps = 4;

/// Nominal bandwidth of each step, in kHz, for display.
extern const uint16_t kBwKhz[kBwSteps];

/// Bytes emitted by build_rx_config(): a flat address/value pair list.
inline constexpr size_t kConfigPairs = 28;
inline constexpr size_t kConfigBytes = kConfigPairs * 2U;

/// True when `hz` falls inside one of the three synthesiser bands.
bool is_tunable(uint32_t hz);

/// Carrier frequency -> the 24-bit FREQ2:FREQ1:FREQ0 word, rounded to nearest.
uint32_t freq_to_word(uint32_t hz);

/// Inverse of freq_to_word(), for reporting where the PLL actually landed.
uint32_t word_to_freq(uint32_t word);

/// Frequency step of one LSB of the FREQ word, in Hz (~397 Hz at 26 MHz).
uint32_t tuning_step_hz();

/// Builds a continuous-receive register set for `hz` into `out`.
///
/// Sync-word detection and packet handling are deliberately disabled: we never
/// decode anything, we only want the receiver to sit in RX so the RSSI register
/// keeps updating. That is also why the beacons being analogue FM does not
/// matter - RSSI reports carrier power regardless of modulation.
///
/// Returns bytes written, or 0 if `hz` is untunable or `out` is too small.
size_t build_rx_config(uint8_t* out, size_t out_len, uint32_t hz, int gain_step, int bw_index);

/// Converts whatever RadioGetRSSI() handed back into dBm.
///
/// The host API documents dBm, and every real reading is negative, so negatives
/// pass through untouched. A positive value is assumed to be the raw CC1101
/// register instead: two's-complement half-dB steps less the datasheet's 74 dB
/// offset for this band.
int normalize_rssi(int raw);

}  // namespace foxhunt::cc1101
