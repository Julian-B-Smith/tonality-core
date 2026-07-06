// Acceptance gate, part 2: reproduce the engine's `set_class_info`
// conformance case within the golden tolerances (read from the fixture,
// currently rel 1e-9 / abs 1e-12). Since slice 1b, EVERY field in the case is
// compared — nothing deferred — and an unrecognized field is a FAILURE, so an
// engine-side surface addition trips this harness instead of slipping by.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "mini_json.hpp"
#include "tonality/chirality.hpp"
#include "tonality/dft.hpp"
#include "tonality/table.hpp"

#ifndef TONALITY_FIXTURES_DIR
#error "TONALITY_FIXTURES_DIR must be defined (see CMakeLists.txt)"
#endif

namespace {

int failures = 0;

void fail(const std::string& what) {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

bool close(double a, double b, double rel, double abs_tol) {
    return std::fabs(a - b) <= std::max(rel * std::max(std::fabs(a), std::fabs(b)), abs_tol);
}

void check_int(const std::string& field, long long expected, long long actual) {
    if (expected != actual) {
        fail(field + ": expected " + std::to_string(expected) + ", got " +
             std::to_string(actual));
    }
}

void check_float(const std::string& field, double expected, double actual, double rel,
                 double abs_tol) {
    if (!close(expected, actual, rel, abs_tol)) {
        fail(field + ": expected " + std::to_string(expected) + ", got " +
             std::to_string(actual));
    }
}

void check_pc_list(const std::string& field, const mini_json::Value& expected,
                   const tonality::PcList& actual) {
    if (expected.array.size() != static_cast<std::size_t>(actual.count)) {
        fail(field + ": length mismatch");
        return;
    }
    for (std::size_t i = 0; i < expected.array.size(); ++i) {
        if (expected.array[i]->integer != actual.pcs[i]) {
            fail(field + "[" + std::to_string(i) + "]: expected " +
                 std::to_string(expected.array[i]->integer) + ", got " +
                 std::to_string(actual.pcs[i]));
        }
    }
}

void check_float_array6(const std::string& field, const mini_json::Value& expected,
                        const std::array<double, 6>& actual, double rel, double abs_tol) {
    if (expected.array.size() != 6) {
        fail(field + ": expected 6 entries");
        return;
    }
    for (std::size_t k = 0; k < 6; ++k) {
        check_float(field + "[" + std::to_string(k) + "]", expected.array[k]->number,
                    actual[k], rel, abs_tol);
    }
}

}  // namespace

int main() {
    const std::string path = std::string(TONALITY_FIXTURES_DIR) + "/conformance.json";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return 2;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const mini_json::ValuePtr root = mini_json::parse(buffer.str());

    const double rel = root->at("float_rel_tol").number;
    const double abs_tol = root->at("float_abs_tol").number;

    const mini_json::Value* found = nullptr;
    for (const auto& case_value : root->at("cases").array) {
        if (case_value->at("tool").string == "set_class_info") {
            found = case_value.get();
            break;
        }
    }
    if (found == nullptr) {
        std::fprintf(stderr, "no set_class_info case in conformance fixture\n");
        return 2;
    }

    tonality::Mask mask = 0;
    for (const auto& pc : found->at("kwargs").at("pcs").array) {
        mask |= static_cast<tonality::Mask>(1u << pc->integer);
    }

    const tonality::SetClassRow row = tonality::compute_row(mask);
    const mini_json::Value& result = found->at("result");

    check_int("mask", result.at("mask").integer, row.mask);
    check_pc_list("normal_order", result.at("normal_order"), row.normal_order);
    check_pc_list("prime_form", result.at("prime_form"), row.prime_form);
    check_int("prime_form_mask", result.at("prime_form_mask").integer, row.prime_form_mask);
    check_int("rotational_period", result.at("rotational_period").integer,
              row.rotational_period);
    check_pc_list("complement_prime_form", result.at("complement_prime_form"),
                  row.complement_prime_form);

    const mini_json::Value& partner = result.at("z_partner_prime_form");
    if (partner.is_null()) {
        if (row.z_partner_prime_form) fail("z_partner_prime_form: expected null");
    } else if (!row.z_partner_prime_form) {
        fail("z_partner_prime_form: expected a partner, got null");
    } else {
        check_pc_list("z_partner_prime_form", partner, *row.z_partner_prime_form);
    }

    check_float_array6("dft_magnitudes", result.at("dft_magnitudes"), row.dft_magnitudes,
                       rel, abs_tol);
    check_float_array6("dft_phases", result.at("dft_phases"), row.dft_phases, rel, abs_tol);

    const mini_json::Value& trichord = result.at("trichord_chirality");
    if (trichord.is_null()) {
        if (row.trichord_chirality) fail("trichord_chirality: expected null");
    } else if (!row.trichord_chirality) {
        fail("trichord_chirality: expected a value, got null");
    } else {
        check_int("trichord_chirality", trichord.integer, *row.trichord_chirality);
    }

    check_float("general_chirality", result.at("general_chirality").number,
                row.general_chirality, rel, abs_tol);
    check_int("chirality_sign", result.at("chirality_sign").integer, row.chirality_sign);
    check_float("chirality", result.at("chirality").number, row.chirality, rel, abs_tol);
    check_float("reflection_residual", result.at("reflection_residual").number,
                row.reflection_residual, rel, abs_tol);

    // Completeness: every field of the case must have been compared above.
    const std::set<std::string> compared = {
        "mask", "normal_order", "prime_form", "prime_form_mask", "rotational_period",
        "complement_prime_form", "z_partner_prime_form", "dft_magnitudes", "dft_phases",
        "trichord_chirality", "general_chirality", "chirality_sign", "chirality",
        "reflection_residual"};
    for (const auto& [field, value] : result.object) {
        if (!compared.count(field)) {
            fail("unrecognized field in conformance case (engine surface grew?): " + field);
        }
    }

    if (failures == 0) {
        std::printf(
            "PARITY OK: set_class_info conformance case reproduced on ALL %zu "
            "fields (mask %d, tolerances rel %g / abs %g)\n",
            result.object.size(), static_cast<int>(mask), rel, abs_tol);
        return 0;
    }
    std::fprintf(stderr, "%d field(s) failed\n", failures);
    return 1;
}
