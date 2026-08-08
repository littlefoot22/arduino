#pragma once

#include <cstdint>

namespace foxhunt {

/// One beacon in the contest.
struct Fox {
    const char* name;
    uint32_t freq_hz;
};

/// The FreeWili's CC1101 synthesises 300-348, 387-464 and 779-928 MHz only.
///
/// Fox 1-5 of the contest sit at 147.420-147.545 MHz. That is roughly 240 MHz
/// below the lowest band the PLL can reach, so no amount of configuration will
/// tune them - they need a separate 2 m receiver. Only the 70 cm foxes, which
/// land comfortably inside the 387-464 MHz band, are listed here.
inline constexpr Fox kFoxes[] = {
    {"FOX 6", 446025000U},
    {"FOX 7", 446050000U},
    {"FOX 8", 446075000U},
    {"FOX 9", 446100000U},
    {"FOX 10", 446125000U},
};

inline constexpr int kFoxCount = static_cast<int>(sizeof(kFoxes) / sizeof(kFoxes[0]));

/// Contest channels are spaced 25 kHz apart but the CC1101 cannot filter
/// narrower than ~58 kHz, so neighbouring foxes always bleed into each other's
/// passband. Readings track the strongest signal in that window rather than one
/// channel in isolation - fine for homing on one fox at a time, not enough to
/// separate two foxes of similar strength.
inline constexpr uint32_t kChannelSpacingHz = 25000U;

}  // namespace foxhunt
