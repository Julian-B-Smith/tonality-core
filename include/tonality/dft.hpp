// DFT of the pc-set characteristic function. Ports mts/core/setclass.py
// (dft_components / dft_magnitudes).
//
// Byte-for-byte parity with the engine's export requires reproducing CPython's
// floating-point results EXACTLY, not just closely: the export serializes with
// shortest-round-trip repr, so a 1-ulp difference changes the bytes. Hence this
// mirrors the engine's evaluation order operation by operation:
//   cmath.exp(-2j*pi*k*pc/12)  ==  (cos(a), sin(a)),  a = (((-2π)·k)·pc)/12
//   (the real part of the exponent is ±0.0, and exp(±0.0) == 1.0),
// summed left-to-right over ascending pcs from an initial 0.0, and
//   abs(complex)  ==  hypot(re, im).
// cos/sin/hypot resolve to the same libm CPython uses on this platform.
#pragma once

#include <array>
#include <cmath>

#include "bitmask.hpp"

namespace tonality {

struct DftComponent {
    double re = 0.0;
    double im = 0.0;
};

// Twice Archimedes' constant, as the engine computes it: -2.0 * math.pi.
inline constexpr double kMinusTwoPi = -6.283185307179586;

// Fourier coefficients f_0..f_6.
inline std::array<DftComponent, 7> dft_components(Mask mask) {
    const PcList pcs = pcs_from_mask(mask);
    std::array<DftComponent, 7> out{};
    for (int k = 0; k < 7; ++k) {
        double re = 0.0;
        double im = 0.0;
        for (int i = 0; i < pcs.count; ++i) {
            const double angle =
                (kMinusTwoPi * k) * pcs.pcs[static_cast<std::size_t>(i)] / 12.0;
            re += std::cos(angle);
            im += std::sin(angle);
        }
        out[static_cast<std::size_t>(k)] = {re, im};
    }
    return out;
}

// |f_1|..|f_6| — the interval-content spectrum (T_n/T_nI-invariant).
inline std::array<double, 6> dft_magnitudes(Mask mask) {
    const std::array<DftComponent, 7> components = dft_components(mask);
    std::array<double, 6> out{};
    for (int k = 1; k < 7; ++k) {
        out[static_cast<std::size_t>(k - 1)] =
            std::hypot(components[static_cast<std::size_t>(k)].re,
                       components[static_cast<std::size_t>(k)].im);
    }
    return out;
}

}  // namespace tonality
