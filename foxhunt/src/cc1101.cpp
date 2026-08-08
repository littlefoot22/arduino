#include "cc1101.h"

namespace foxhunt::cc1101 {
namespace {

// Register addresses (CC1101 datasheet, section 29).
constexpr uint8_t kFsctrl1 = 0x0B;
constexpr uint8_t kFsctrl0 = 0x0C;
constexpr uint8_t kFreq2 = 0x0D;
constexpr uint8_t kFreq1 = 0x0E;
constexpr uint8_t kFreq0 = 0x0F;
constexpr uint8_t kMdmcfg4 = 0x10;
constexpr uint8_t kMdmcfg3 = 0x11;
constexpr uint8_t kMdmcfg2 = 0x12;
constexpr uint8_t kMdmcfg1 = 0x13;
constexpr uint8_t kMdmcfg0 = 0x14;
constexpr uint8_t kDeviatn = 0x15;
constexpr uint8_t kMcsm2 = 0x16;
constexpr uint8_t kMcsm1 = 0x17;
constexpr uint8_t kMcsm0 = 0x18;
constexpr uint8_t kFoccfg = 0x19;
constexpr uint8_t kBscfg = 0x1A;
constexpr uint8_t kAgcctrl2 = 0x1B;
constexpr uint8_t kAgcctrl1 = 0x1C;
constexpr uint8_t kAgcctrl0 = 0x1D;
constexpr uint8_t kFrend1 = 0x21;
constexpr uint8_t kFrend0 = 0x22;
constexpr uint8_t kFscal3 = 0x23;
constexpr uint8_t kFscal2 = 0x24;
constexpr uint8_t kFscal1 = 0x25;
constexpr uint8_t kFscal0 = 0x26;
constexpr uint8_t kTest2 = 0x2C;
constexpr uint8_t kTest1 = 0x2D;
constexpr uint8_t kTest0 = 0x2E;

/// AGCCTRL2: MAX_DVGA_GAIN[7:6] | MAX_LNA_GAIN[5:3] | MAGN_TARGET[2:0].
/// Backing DVGA off first costs ~7 dB a step; the last entry also drops the LNA
/// by ~17 dB for when the beacon is close enough to swamp the front end.
constexpr uint8_t kAgcCtrl2Table[kGainSteps] = {
    0x03,  // full sensitivity
    0x43,  // -1 DVGA step
    0x83,  // -2 DVGA steps
    0xC3,  // -3 DVGA steps
    0xFB,  // -3 DVGA steps and reduced LNA
};

/// MDMCFG4[7:6] = CHANBW_E, [5:4] = CHANBW_M.
/// BW = fxosc / (8 * (4 + CHANBW_M) * 2^CHANBW_E).
constexpr uint8_t kChanBwTable[kBwSteps] = {
    0xF0,  // E=3 M=3 -> 58.0 kHz
    0xE0,  // E=3 M=2 -> 67.7 kHz
    0xD0,  // E=3 M=1 -> 81.3 kHz
    0xC0,  // E=3 M=0 -> 101.6 kHz
};

/// Data rate is irrelevant when nothing is demodulated, but the exponent shares
/// MDMCFG4 with the bandwidth field, so it still has to be written.
constexpr uint8_t kDrateExponent = 0x08;
constexpr uint8_t kDrateMantissa = 0x93;

int clamp(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

/// Appends one address/value pair and advances the cursor.
void emit(uint8_t* out, size_t& at, uint8_t reg, uint8_t value) {
    out[at] = reg;
    out[at + 1U] = value;
    at += 2U;
}

}  // namespace

const uint16_t kBwKhz[kBwSteps] = {58U, 68U, 81U, 102U};

bool is_tunable(uint32_t hz) {
    for (int i = 0; i < kBandCount; ++i) {
        if (hz >= kBands[i].low_hz && hz <= kBands[i].high_hz) {
            return true;
        }
    }
    return false;
}

uint32_t freq_to_word(uint32_t hz) {
    // FREQ[23:0] = f_carrier * 2^16 / f_xosc. Done in 64-bit integers so the
    // result is exact and reproducible rather than float-rounded.
    const uint64_t numerator = static_cast<uint64_t>(hz) << 16U;
    const uint64_t word = (numerator + (kXoscHz / 2U)) / kXoscHz;
    return static_cast<uint32_t>(word & 0xFFFFFFU);
}

uint32_t word_to_freq(uint32_t word) {
    const uint64_t hz = ((static_cast<uint64_t>(word) * kXoscHz) + (1ULL << 15U)) >> 16U;
    return static_cast<uint32_t>(hz);
}

uint32_t tuning_step_hz() {
    return (kXoscHz + (1U << 15U)) >> 16U;
}

size_t build_rx_config(uint8_t* out, size_t out_len, uint32_t hz, int gain_step, int bw_index) {
    if (out == nullptr || out_len < kConfigBytes || !is_tunable(hz)) {
        return 0U;
    }

    const int gain = clamp(gain_step, 0, kGainSteps - 1);
    const int bw = clamp(bw_index, 0, kBwSteps - 1);
    const uint32_t word = freq_to_word(hz);

    size_t at = 0U;

    // Intermediate frequency of ~152 kHz, matched by FSCTRL1.
    emit(out, at, kFsctrl1, 0x06);
    emit(out, at, kFsctrl0, 0x00);

    emit(out, at, kFreq2, static_cast<uint8_t>((word >> 16U) & 0xFFU));
    emit(out, at, kFreq1, static_cast<uint8_t>((word >> 8U) & 0xFFU));
    emit(out, at, kFreq0, static_cast<uint8_t>(word & 0xFFU));

    emit(out, at, kMdmcfg4, static_cast<uint8_t>(kChanBwTable[bw] | kDrateExponent));
    emit(out, at, kMdmcfg3, kDrateMantissa);
    // 0x30: ASK/OOK, no Manchester, and crucially SYNC_MODE = 000 so the
    // receiver never waits for a sync word before reporting signal.
    emit(out, at, kMdmcfg2, 0x30);
    emit(out, at, kMdmcfg1, 0x22);
    emit(out, at, kMdmcfg0, 0xF8);
    emit(out, at, kDeviatn, 0x47);

    // RX_TIME = 111 disables the receive timeout: the receiver stays open
    // indefinitely, which is what continuous RSSI sampling needs.
    emit(out, at, kMcsm2, 0x07);
    // Return to RX after a packet instead of dropping to IDLE.
    emit(out, at, kMcsm1, 0x3C);
    // Auto-calibrate the synthesiser when leaving IDLE, so retuning between
    // channels does not need an explicit calibration strobe.
    emit(out, at, kMcsm0, 0x18);

    emit(out, at, kFoccfg, 0x16);
    emit(out, at, kBscfg, 0x6C);

    emit(out, at, kAgcctrl2, kAgcCtrl2Table[gain]);
    emit(out, at, kAgcctrl1, 0x40);
    emit(out, at, kAgcctrl0, 0x91);

    emit(out, at, kFrend1, 0x56);
    emit(out, at, kFrend0, 0x10);

    emit(out, at, kFscal3, 0xE9);
    emit(out, at, kFscal2, 0x2A);
    emit(out, at, kFscal1, 0x00);
    emit(out, at, kFscal0, 0x1F);

    emit(out, at, kTest2, 0x81);
    emit(out, at, kTest1, 0x35);
    emit(out, at, kTest0, 0x09);

    return at;
}

int normalize_rssi(int raw) {
    if (raw <= 0) {
        return raw;
    }
    int value = raw & 0xFF;
    if (value >= 128) {
        value -= 256;
    }
    return (value / 2) - 74;
}

}  // namespace foxhunt::cc1101
