// Slice 1b: DFT phases + the chirality family. Ports mts/core/setclass.py
// (dft_phases, general_chirality, chirality_sign, reflection_residual,
// chirality) and mts/analysis/pcset_math.py (trichord_chirality).
//
// Same byte-parity doctrine as dft.hpp: CPython's evaluation is replicated
// operation-for-operation —
//   complex mul/conj/pow use CPython's exact formulas (c_powu builds x², x³
//   as x·x and x·(x·x)); cmath.phase == atan2(im, re); abs(z)**2 ==
//   pow(hypot(re, im), 2.0); round(x, 10) is Python's correctly-rounded
//   decimal rounding (reproduced via the platform's correctly-rounded
//   printf/strtod pair); max(x, 0.0) keeps -0.0 (0.0 > -0.0 is false).
// All libm calls resolve to the same functions CPython uses on this platform.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "bitmask.hpp"
#include "dft.hpp"

namespace tonality {

// CPython's _Py_c_prod as it is actually COMPILED on this platform: clang
// builds CPython with FMA contraction on, fusing the first (LHS) multiply of
// each component into the add/sub and rounding the second separately —
// verified bit-for-bit against the interpreter (mask 108 was the witness:
// naive double rounding lands the reflection residual on -0.0 where the
// engine says 0.0). Interpreter-LEVEL Python arithmetic is never fused
// (one rounding per bytecode), which is why this repo compiles with
// -ffp-contract=off and encodes CPython's C-layer fusion explicitly here.
inline DftComponent cmul(DftComponent a, DftComponent b) {
    return {std::fma(a.re, b.re, -(a.im * b.im)),
            std::fma(a.re, b.im, a.im * b.re)};
}

inline DftComponent cconj(DftComponent a) { return {a.re, -a.im}; }

// complex ** n via CPython's c_powu, replicated literally — including the
// seed multiplication by complex 1, which under the fused c_prod is NOT an
// identity for signed zeros (fma(1, z.im, 0.0*z.re) can turn -0.0 into +0.0),
// and the engine's ±0.0 residuals hang off exactly such bits.
inline DftComponent cpowu(DftComponent x, int n) {
    DftComponent r{1.0, 0.0};
    DftComponent p = x;
    int mask = 1;
    while (mask > 0 && n >= mask) {
        if (n & mask) r = cmul(r, p);
        mask <<= 1;
        p = cmul(p, p);
    }
    return r;
}

inline DftComponent csquare(DftComponent x) { return cpowu(x, 2); }
inline DftComponent ccube(DftComponent x) { return cpowu(x, 3); }

// The actual libm pow, forced through a volatile function pointer so LLVM
// cannot fold pow(x, 2.0) into x*x or evaluate pow(5.0, 0.5) at compile time.
// This matters: Apple's pow is NOT correctly rounded everywhere (e.g.
// pow(3.0000000000000004, 2.0) is 1 ulp off the true square), and CPython's
// float.__pow__ calls libm pow at runtime — parity means matching libm's
// answer, not the mathematically better one.
inline double libm_pow(double base, double exponent) {
    static double (*volatile pow_fn)(double, double) = ::pow;
    return pow_fn(base, exponent);
}

// Python round(value, 10): correctly-rounded decimal rounding to 10
// fractional digits. The printf/strtod round-trip reproduces CPython's
// dtoa-based double_round for the magnitudes this library produces (|x| far
// below the %.10f buffer limit); the byte-for-byte table gate arbitrates.
inline double py_round_10(double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.10f", value);
    return std::strtod(buf, nullptr);
}

// arg(f_1)..arg(f_6) in radians — NOT a set-class invariant (rotates under
// T_n, negates under inversion); reported for the literal mask.
inline std::array<double, 6> dft_phases(Mask mask) {
    const std::array<DftComponent, 7> components = dft_components(mask);
    std::array<double, 6> out{};
    for (int k = 1; k < 7; ++k) {
        out[static_cast<std::size_t>(k - 1)] =
            std::atan2(components[static_cast<std::size_t>(k)].im,
                       components[static_cast<std::size_t>(k)].re);
    }
    return out;
}

// Step-gap chirality of a trichord: (a−b)(b−c)(c−a) over the three circular
// step-gaps; nullopt for any non-trichord.
inline std::optional<int> trichord_chirality(Mask mask) {
    const PcList pcs = pcs_from_mask(mask);
    if (pcs.count != 3) return std::nullopt;
    const int p0 = pcs.pcs[0], p1 = pcs.pcs[1], p2 = pcs.pcs[2];  // ascending
    const int a = p1 - p0, b = p2 - p1, c = (p0 + 12) - p2;
    return (a - b) * (b - c) * (c - a);
}

// Im(f_1·f_2·conj(f_3)) — the smooth bispectrum-slice handedness scalar,
// snapped to 10 dp with -0.0 normalized (major < 0 / minor > 0).
inline double general_chirality(Mask mask) {
    const std::array<DftComponent, 7> c = dft_components(mask);
    const double value = cmul(cmul(c[1], c[2]), cconj(c[3])).im;
    return py_round_10(value) + 0.0;
}

// The canonical inversion-odd slice family, generated exactly as the engine
// generates it: one representative per ±mirror pair, (1, 2) first so the sign
// agrees with general_chirality, then lexicographic.
inline const std::vector<std::pair<int, int>>& chirality_slices() {
    static const std::vector<std::pair<int, int>> slices = [] {
        std::vector<std::pair<int, int>> pairs;
        for (int a = 1; a < 12; ++a) {
            for (int b = a; b < 12; ++b) {
                const int ma = (12 - a) % 12, mb = (12 - b) % 12;
                const std::pair<int, int> mirror{std::min(ma, mb), std::max(ma, mb)};
                if (std::pair<int, int>{a, b} <= mirror) pairs.emplace_back(a, b);
            }
        }
        std::sort(pairs.begin(), pairs.end(),
                  [](const std::pair<int, int>& x, const std::pair<int, int>& y) {
                      const bool xk = x != std::pair<int, int>{1, 2};
                      const bool yk = y != std::pair<int, int>{1, 2};
                      return std::tie(xk, x) < std::tie(yk, y);
                  });
        return pairs;
    }();
    return slices;
}

inline constexpr double kChiralityEps = 1e-7;

// Complete handedness: -1 / 0 / +1, 0 iff achiral. First nonzero member of
// the slice family, then the f_1³·conj(f_3) trispectrum fallback.
inline int chirality_sign(Mask mask) {
    const std::array<DftComponent, 7> comp = dft_components(mask);
    std::array<DftComponent, 12> f{};
    for (int k = 0; k < 7; ++k) f[static_cast<std::size_t>(k)] = comp[static_cast<std::size_t>(k)];
    for (int k = 7; k < 12; ++k) {
        f[static_cast<std::size_t>(k)] = cconj(comp[static_cast<std::size_t>(12 - k)]);
    }
    for (const auto& [a, b] : chirality_slices()) {
        const double value =
            cmul(cmul(f[static_cast<std::size_t>(a)], f[static_cast<std::size_t>(b)]),
                 cconj(f[static_cast<std::size_t>((a + b) % 12)]))
                .im;
        if (std::fabs(value) > kChiralityEps) return value < 0 ? -1 : 1;
    }
    const double value = cmul(ccube(f[1]), cconj(f[3])).im;
    if (std::fabs(value) > kChiralityEps) return value < 0 ? -1 : 1;
    return 0;
}

// CPython 3.12+ builtin sum() over floats — Neumaier compensated summation
// (bltinmodule.c float fast path). sum(<genexp of floats>) starts from int 0,
// so the first item joins by a plain add and compensation begins at the
// second; the return is f + c. The engine's reflection_residual uses sum()
// for both of its float reductions, so naive left-to-right accumulation
// diverges by an ulp — enough to flip an achiral set's ±0.0.
// (Complex sums — dft_components — take no fast path and stay naive.)
class PyFloatSum {
public:
    void add(double x) {
        if (first_) {
            f_ = 0.0 + x;
            first_ = false;
            return;
        }
        const double t = f_ + x;
        if (std::fabs(f_) >= std::fabs(x)) {
            c_ += (f_ - t) + x;
        } else {
            c_ += (x - t) + f_;
        }
        f_ = t;
    }
    double result() const { return f_ + c_; }

private:
    double f_ = 0.0;
    double c_ = 0.0;
    bool first_ = true;
};

inline constexpr int kReflectionGrid = 360;

// Best-fit reflection-axis asymmetry R = min_θ Σ|f_k|²·sin²(φ_k+kθ); 0 iff
// achiral. Grid bracket + golden-section refine, replicated step-for-step
// (including Python max(round(R, 10), 0.0), which passes -0.0 through).
inline double reflection_residual(Mask mask) {
    const std::array<DftComponent, 7> comp = dft_components(mask);
    std::array<DftComponent, 7> squares{};
    for (int k = 0; k < 7; ++k) {
        squares[static_cast<std::size_t>(k)] = csquare(comp[static_cast<std::size_t>(k)]);
    }
    PyFloatSum acc;
    for (int k = 1; k < 7; ++k) {
        acc.add(libm_pow(std::hypot(comp[static_cast<std::size_t>(k)].re,
                                    comp[static_cast<std::size_t>(k)].im),
                         2.0));
    }
    const double constant = acc.result() / 2.0;

    const auto residual = [&](double theta) {
        PyFloatSum spectrum;
        for (int k = 1; k < 7; ++k) {
            const double y = (2.0 * k) * theta;
            // (squares[k] * cmath.exp(2ikθ)).real — the c_prod real part,
            // with CPython's compiled-in fusion (see cmul).
            spectrum.add(std::fma(squares[static_cast<std::size_t>(k)].re, std::cos(y),
                                  -(squares[static_cast<std::size_t>(k)].im * std::sin(y))));
        }
        return constant - 0.5 * spectrum.result();
    };

    const double pi = 3.141592653589793;  // cmath.pi
    const double step = pi / kReflectionGrid;
    int best = 0;
    double best_value = residual(0.0 * step);
    for (int i = 1; i < kReflectionGrid; ++i) {
        const double value = residual(i * step);
        if (value < best_value) {  // strict: Python min keeps the first tie
            best_value = value;
            best = i;
        }
    }
    const double centre = best * step;
    const double golden = (libm_pow(5.0, 0.5) - 1) / 2;  // (5**0.5 - 1) / 2
    double lo = centre - step, hi = centre + step;
    double c = hi - golden * (hi - lo);
    double d = lo + golden * (hi - lo);
    for (int i = 0; i < 60; ++i) {
        if (residual(c) < residual(d)) {
            hi = d;
            d = c;
            c = hi - golden * (hi - lo);
        } else {
            lo = c;
            c = d;
            d = lo + golden * (hi - lo);
        }
    }
    const double rounded = py_round_10(residual((lo + hi) / 2.0));
    return 0.0 > rounded ? 0.0 : rounded;  // Python max(): -0.0 passes through
}

// Complete signed continuous chirality: chirality_sign · √R, 10 dp.
inline double chirality(Mask mask) {
    const int sign = chirality_sign(mask);
    if (sign == 0) return 0.0;
    return py_round_10(sign * std::sqrt(reflection_residual(mask))) + 0.0;
}

}  // namespace tonality
