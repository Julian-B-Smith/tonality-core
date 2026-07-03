// 12-bit pitch-class-set mask operations — the substrate of the identity layer.
// Ports mts/core/bitmask.py; the parity harness holds it to the engine's
// exported set_class_table.json byte-for-byte.
#pragma once

#include <array>
#include <bit>
#include <cstdint>

namespace tonality {

using Mask = std::uint16_t;

inline constexpr Mask kFullMask = 0xFFF;

// Ascending pitch classes of a mask, with an explicit count (fixed-size so the
// whole identity layer stays allocation-free and constexpr-capable).
struct PcList {
    std::array<std::uint8_t, 12> pcs{};
    int count = 0;
};

constexpr PcList pcs_from_mask(Mask mask) {
    PcList out;
    for (int pc = 0; pc < 12; ++pc) {
        if (mask & (Mask{1} << pc)) {
            out.pcs[static_cast<std::size_t>(out.count++)] = static_cast<std::uint8_t>(pc);
        }
    }
    return out;
}

constexpr int cardinality(Mask mask) { return std::popcount(mask); }

constexpr bool is_subset(Mask subset_mask, Mask superset_mask) {
    return (subset_mask & superset_mask) == subset_mask;
}

// T_n: every pc moves up by `semitones` (mod 12; negatives allowed, matching
// Python's floored modulo).
constexpr Mask rotate_mask(Mask mask, int semitones) {
    const int shift = ((semitones % 12) + 12) % 12;
    Mask rotated = 0;
    for (int pc = 0; pc < 12; ++pc) {
        if (mask & (Mask{1} << pc)) {
            rotated |= Mask{1} << ((pc + shift) % 12);
        }
    }
    return rotated;
}

// I_n: each pc maps to (index - pc) mod 12.
constexpr Mask invert_mask(Mask mask, int index = 0) {
    Mask inverted = 0;
    for (int pc = 0; pc < 12; ++pc) {
        if (mask & (Mask{1} << pc)) {
            inverted |= Mask{1} << ((((index - pc) % 12) + 12) % 12);
        }
    }
    return inverted;
}

constexpr Mask complement_mask(Mask mask) { return static_cast<Mask>(~mask) & kFullMask; }

// Interval-class vector (6 counts, ic1..ic6).
constexpr std::array<int, 6> interval_vector(Mask mask) {
    std::array<int, 6> vector{};
    const PcList pcs = pcs_from_mask(mask);
    for (int i = 0; i < pcs.count; ++i) {
        for (int j = i + 1; j < pcs.count; ++j) {
            const int diff = (pcs.pcs[static_cast<std::size_t>(j)] - pcs.pcs[static_cast<std::size_t>(i)] + 12) % 12;
            const int ic = diff <= 6 ? diff : 12 - diff;
            ++vector[static_cast<std::size_t>(ic - 1)];
        }
    }
    return vector;
}

// Smallest transposition (1..12) mapping the set to itself; 12 = no nontrivial
// rotational symmetry. Ports mts/core/symmetry.py rotational_period.
constexpr int rotational_period(Mask mask) {
    for (int step = 1; step < 12; ++step) {
        if (rotate_mask(mask, step) == mask) return step;
    }
    return 12;
}

}  // namespace tonality
