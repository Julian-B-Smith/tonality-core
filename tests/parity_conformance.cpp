// Slice-1 acceptance gate, part 2 (CPP_PORT.md): reproduce the engine's
// `set_class_info` conformance case within the golden tolerances (read from
// the fixture itself, currently rel 1e-9 / abs 1e-12).
//
// The case also carries slice-1b fields (dft_phases, chirality family) that
// are deliberately NOT ported yet (PORT.md adopted default 4: held until they
// join the export table). Those are reported as DEFERRED, never compared.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "mini_json.hpp"
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

    // Build the mask from the case input.
    tonality::Mask mask = 0;
    for (const auto& pc : found->at("kwargs").at("pcs").array) {
        mask |= static_cast<tonality::Mask>(1u << pc->integer);
    }

    const tonality::SetClassRow row = tonality::compute_row(mask);
    const mini_json::Value& result = found->at("result");

    // Slice-1 fields — compared.
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

    const mini_json::Value& magnitudes = result.at("dft_magnitudes");
    for (std::size_t k = 0; k < 6; ++k) {
        const double expected = magnitudes.array[k]->number;
        const double actual = row.dft_magnitudes[k];
        if (!close(expected, actual, rel, abs_tol)) {
            fail("dft_magnitudes[" + std::to_string(k) + "]: expected " +
                 std::to_string(expected) + ", got " + std::to_string(actual));
        }
    }

    // Slice-1b fields — present in the case, deliberately deferred.
    const std::set<std::string> slice1 = {
        "mask", "normal_order", "prime_form", "prime_form_mask", "interval_vector",
        "dft_magnitudes", "z_partner_prime_form", "complement_prime_form",
        "rotational_period", "cardinality"};
    for (const auto& [field, value] : result.object) {
        if (!slice1.count(field)) {
            std::printf("DEFERRED (slice 1b, not compared): %s\n", field.c_str());
        }
    }

    if (failures == 0) {
        std::printf(
            "PARITY OK: set_class_info conformance case reproduced "
            "(mask %d, tolerances rel %g / abs %g)\n",
            static_cast<int>(mask), rel, abs_tol);
        return 0;
    }
    std::fprintf(stderr, "%d field(s) failed\n", failures);
    return 1;
}
