// The exported set-class table, computed by this core.
//
// Row semantics and JSON bytes mirror mts/io/export.py set_class_table() +
// scripts/export_versioned_data.py exactly: one row per mask 0..4095 (list
// position == mask), keys in SET_CLASS_TABLE_FIELDS order, compact separators
// (",", ":"), trailing newline. The parity harness diffs the emitted document
// byte-for-byte against the engine's vendored export.
#pragma once

#include <array>
#include <optional>
#include <string>

#include "bitmask.hpp"
#include "chirality.hpp"
#include "dft.hpp"
#include "json_format.hpp"
#include "setclass.hpp"

namespace tonality {

struct SetClassRow {
    Mask mask = 0;
    int cardinality = 0;
    PcList normal_order;
    PcList prime_form;
    Mask prime_form_mask = 0;
    std::array<int, 6> interval_vector{};
    std::array<double, 6> dft_magnitudes{};
    std::optional<PcList> z_partner_prime_form;
    PcList complement_prime_form;
    int rotational_period = 12;
    // slice-1b family (export.2)
    std::array<double, 6> dft_phases{};
    std::optional<int> trichord_chirality;
    double general_chirality = 0.0;
    int chirality_sign = 0;
    double chirality = 0.0;
    double reflection_residual = 0.0;
};

inline SetClassRow compute_row(Mask mask) {
    SetClassRow row;
    row.mask = mask;
    row.cardinality = cardinality(mask);
    row.normal_order = normal_order(mask);
    row.prime_form = prime_form(mask);
    row.prime_form_mask = prime_form_mask(mask);
    row.interval_vector = interval_vector(mask);
    row.dft_magnitudes = dft_magnitudes(mask);
    if (const std::optional<Mask> partner = z_partner_mask(mask)) {
        row.z_partner_prime_form = pcs_from_mask(*partner);
    }
    row.complement_prime_form = prime_form(complement_mask(mask));
    row.rotational_period = rotational_period(mask);
    row.dft_phases = tonality::dft_phases(mask);
    row.trichord_chirality = tonality::trichord_chirality(mask);
    row.general_chirality = tonality::general_chirality(mask);
    row.chirality_sign = tonality::chirality_sign(mask);
    row.reflection_residual = tonality::reflection_residual(mask);
    // chirality = sign·√R over the same (rounded) residual — reuse it rather
    // than re-running the minimizer; identical to the engine's cached calls.
    row.chirality = row.chirality_sign == 0
                        ? 0.0
                        : py_round_10(row.chirality_sign *
                                      std::sqrt(row.reflection_residual)) +
                              0.0;
    return row;
}

namespace detail {

inline void append_pc_array(std::string& out, const PcList& pcs) {
    out += '[';
    for (int i = 0; i < pcs.count; ++i) {
        if (i) out += ',';
        out += std::to_string(pcs.pcs[static_cast<std::size_t>(i)]);
    }
    out += ']';
}

}  // namespace detail

inline void append_row_json(std::string& out, const SetClassRow& row) {
    out += "{\"mask\":";
    out += std::to_string(row.mask);
    out += ",\"cardinality\":";
    out += std::to_string(row.cardinality);
    out += ",\"normal_order\":";
    detail::append_pc_array(out, row.normal_order);
    out += ",\"prime_form\":";
    detail::append_pc_array(out, row.prime_form);
    out += ",\"prime_form_mask\":";
    out += std::to_string(row.prime_form_mask);
    out += ",\"interval_vector\":[";
    for (int i = 0; i < 6; ++i) {
        if (i) out += ',';
        out += std::to_string(row.interval_vector[static_cast<std::size_t>(i)]);
    }
    out += "],\"dft_magnitudes\":[";
    for (int i = 0; i < 6; ++i) {
        if (i) out += ',';
        if (row.cardinality == 0) {
            // The engine's sum() over an empty pc list is the int 0, so mask 0
            // serializes its magnitudes as JSON ints — reproduce that.
            out += '0';
        } else {
            append_python_float_repr(out, row.dft_magnitudes[static_cast<std::size_t>(i)]);
        }
    }
    out += "],\"z_partner_prime_form\":";
    if (row.z_partner_prime_form) {
        detail::append_pc_array(out, *row.z_partner_prime_form);
    } else {
        out += "null";
    }
    out += ",\"complement_prime_form\":";
    detail::append_pc_array(out, row.complement_prime_form);
    out += ",\"rotational_period\":";
    out += std::to_string(row.rotational_period);
    out += ",\"dft_phases\":[";
    for (int i = 0; i < 6; ++i) {
        if (i) out += ',';
        append_python_float_repr(out, row.dft_phases[static_cast<std::size_t>(i)]);
    }
    out += "],\"trichord_chirality\":";
    if (row.trichord_chirality) {
        out += std::to_string(*row.trichord_chirality);
    } else {
        out += "null";
    }
    out += ",\"general_chirality\":";
    append_python_float_repr(out, row.general_chirality);
    out += ",\"chirality_sign\":";
    out += std::to_string(row.chirality_sign);
    out += ",\"chirality\":";
    append_python_float_repr(out, row.chirality);
    out += ",\"reflection_residual\":";
    append_python_float_repr(out, row.reflection_residual);
    out += '}';
}

// The full set_class_table.json document, byte-identical to the engine export.
inline std::string emit_table_json() {
    std::string out;
    out.reserve(1500000);
    out += '[';
    for (int mask = 0; mask <= kFullMask; ++mask) {
        if (mask) out += ',';
        append_row_json(out, compute_row(static_cast<Mask>(mask)));
    }
    out += "]\n";
    return out;
}

}  // namespace tonality
