// pybind11 fast path into the C++ core (Tonality Decision 10, revised): an
// OPTIONAL acceleration for Python consumers, never a replacement for the
// pure-Python engine. The surface mirrors slice 1 exactly — the exported
// SET_CLASS_TABLE_FIELDS — nothing more (port-by-stability fence).
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "tonality/bitmask.hpp"
#include "tonality/chirality.hpp"
#include "tonality/dft.hpp"
#include "tonality/setclass.hpp"
#include "tonality/table.hpp"

namespace py = pybind11;

namespace {

py::list to_list(const tonality::PcList& pcs) {
    py::list out;
    for (int i = 0; i < pcs.count; ++i) out.append(static_cast<int>(pcs.pcs[static_cast<std::size_t>(i)]));
    return out;
}

tonality::Mask checked_mask(int mask) {
    if (mask < 0 || mask > tonality::kFullMask) {
        throw py::value_error("mask out of range 0..4095: " + std::to_string(mask));
    }
    return static_cast<tonality::Mask>(mask);
}

tonality::Mask mask_from_pcs(const std::vector<int>& pcs) {
    tonality::Mask mask = 0;
    for (int pc : pcs) {
        if (pc < 0 || pc > 11) {
            throw py::value_error("pitch class out of range: " + std::to_string(pc));
        }
        mask |= tonality::Mask{1} << pc;
    }
    return mask;
}

// One table row as a dict — same keys, order, and value shapes as the engine's
// set_class_table() rows (dft_magnitudes are floats; the engine's mask-0 int
// quirk is a JSON-serialization artifact, and 0 == 0.0 in Python).
py::dict set_class_row(int mask_in) {
    const tonality::SetClassRow row = tonality::compute_row(checked_mask(mask_in));
    py::dict out;
    out["mask"] = static_cast<int>(row.mask);
    out["cardinality"] = row.cardinality;
    out["normal_order"] = to_list(row.normal_order);
    out["prime_form"] = to_list(row.prime_form);
    out["prime_form_mask"] = static_cast<int>(row.prime_form_mask);
    py::list vector;
    for (int v : row.interval_vector) vector.append(v);
    out["interval_vector"] = vector;
    py::list magnitudes;
    for (double m : row.dft_magnitudes) magnitudes.append(m);
    out["dft_magnitudes"] = magnitudes;
    out["z_partner_prime_form"] =
        row.z_partner_prime_form ? py::object(to_list(*row.z_partner_prime_form))
                                 : py::object(py::none());
    out["complement_prime_form"] = to_list(row.complement_prime_form);
    out["rotational_period"] = row.rotational_period;
    py::list phases;
    for (double p : row.dft_phases) phases.append(p);
    out["dft_phases"] = phases;
    out["trichord_chirality"] = row.trichord_chirality
                                    ? py::object(py::int_(*row.trichord_chirality))
                                    : py::object(py::none());
    out["general_chirality"] = row.general_chirality;
    out["chirality_sign"] = row.chirality_sign;
    out["chirality"] = row.chirality;
    out["reflection_residual"] = row.reflection_residual;
    return out;
}

}  // namespace

PYBIND11_MODULE(tonality_core, m) {
    m.doc() =
        "Native fast path into tonality-core (slice 1: the identity layer). "
        "The pure-Python Tonality engine remains the spec's source of truth; "
        "this module reproduces its exported set-class table exactly.";

    m.def("mask_from_pcs", &mask_from_pcs, py::arg("pcs"),
          "12-bit mask from an iterable of pitch classes (0..11).");
    m.def("pcs_from_mask",
          [](int mask) { return to_list(tonality::pcs_from_mask(checked_mask(mask))); },
          py::arg("mask"), "Ascending pitch classes of a mask.");
    m.def("cardinality", [](int mask) { return tonality::cardinality(checked_mask(mask)); },
          py::arg("mask"));
    m.def("is_subset",
          [](int sub, int super) {
              return tonality::is_subset(checked_mask(sub), checked_mask(super));
          },
          py::arg("subset_mask"), py::arg("superset_mask"));
    m.def("rotate_mask",
          [](int mask, int semitones) {
              return static_cast<int>(tonality::rotate_mask(checked_mask(mask), semitones));
          },
          py::arg("mask"), py::arg("semitones"), "T_n transposition of a mask.");
    m.def("invert_mask",
          [](int mask, int index) {
              return static_cast<int>(tonality::invert_mask(checked_mask(mask), index));
          },
          py::arg("mask"), py::arg("index") = 0, "I_n inversion of a mask.");
    m.def("complement_mask",
          [](int mask) { return static_cast<int>(tonality::complement_mask(checked_mask(mask))); },
          py::arg("mask"));
    m.def("interval_vector",
          [](int mask) {
              py::list out;
              for (int v : tonality::interval_vector(checked_mask(mask))) out.append(v);
              return out;
          },
          py::arg("mask"), "Interval-class vector (ic1..ic6).");
    m.def("rotational_period",
          [](int mask) { return tonality::rotational_period(checked_mask(mask)); },
          py::arg("mask"),
          "Smallest transposition (1..12) mapping the set to itself; 12 = none.");
    m.def("normal_order",
          [](int mask) { return to_list(tonality::normal_order(checked_mask(mask))); },
          py::arg("mask"), "Rahn normal order (actual pitch classes).");
    m.def("prime_form",
          [](int mask) { return to_list(tonality::prime_form(checked_mask(mask))); },
          py::arg("mask"), "Rahn prime form as a zero-based pc list.");
    m.def("prime_form_mask",
          [](int mask) { return static_cast<int>(tonality::prime_form_mask(checked_mask(mask))); },
          py::arg("mask"), "Rahn prime form as a mask (Ian Ring convention).");
    m.def("z_partner_mask",
          [](int mask) -> py::object {
              const auto partner = tonality::z_partner_mask(checked_mask(mask));
              if (!partner) return py::none();
              return py::int_(static_cast<int>(*partner));
          },
          py::arg("mask"),
          "Prime-form mask of the Z-partner, or None if not Z-related.");
    m.def("dft_magnitudes",
          [](int mask) {
              py::list out;
              for (double v : tonality::dft_magnitudes(checked_mask(mask))) out.append(v);
              return out;
          },
          py::arg("mask"), "|f_1|..|f_6| of the pc-set characteristic function.");
    m.def("dft_phases",
          [](int mask) {
              py::list out;
              for (double v : tonality::dft_phases(checked_mask(mask))) out.append(v);
              return out;
          },
          py::arg("mask"), "arg(f_1)..arg(f_6) in radians (not a set-class invariant).");
    m.def("trichord_chirality",
          [](int mask) -> py::object {
              const auto value = tonality::trichord_chirality(checked_mask(mask));
              if (!value) return py::none();
              return py::int_(*value);
          },
          py::arg("mask"), "Step-gap chirality of a trichord; None otherwise.");
    m.def("general_chirality",
          [](int mask) { return tonality::general_chirality(checked_mask(mask)); },
          py::arg("mask"), "Im(f_1·f_2·conj(f_3)) — smooth handedness scalar.");
    m.def("chirality_sign",
          [](int mask) { return tonality::chirality_sign(checked_mask(mask)); },
          py::arg("mask"), "Complete handedness: -1/0/+1, 0 iff achiral.");
    m.def("chirality",
          [](int mask) { return tonality::chirality(checked_mask(mask)); },
          py::arg("mask"), "Complete signed continuous chirality: sign · sqrt(R).");
    m.def("reflection_residual",
          [](int mask) { return tonality::reflection_residual(checked_mask(mask)); },
          py::arg("mask"), "Best-fit reflection-axis asymmetry R; 0 iff achiral.");
    m.def("set_class_row", &set_class_row, py::arg("mask"),
          "Full row for a mask — identical to the engine's exported "
          "set_class_table() entry (export.2: slices 1 + 1b).");
    m.def("emit_table_json", &tonality::emit_table_json,
          "The complete set_class_table.json document, byte-identical to the "
          "engine's export.");
}
