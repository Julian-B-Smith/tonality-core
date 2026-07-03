// Slice-1 acceptance gate, part 1 (CPP_PORT.md): regenerate
// set_class_table.json from this core and diff it BYTE-FOR-BYTE against the
// engine's vendored export (4096 rows). Any difference is a port bug — Python
// is the spec.
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "tonality/table.hpp"

#ifndef TONALITY_FIXTURES_DIR
#error "TONALITY_FIXTURES_DIR must be defined (see CMakeLists.txt)"
#endif

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        std::exit(2);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}  // namespace

int main() {
    const std::string fixture_path =
        std::string(TONALITY_FIXTURES_DIR) + "/set_class_table.json";
    const std::string expected = read_file(fixture_path);

    const auto start = std::chrono::steady_clock::now();
    const std::string actual = tonality::emit_table_json();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();

    if (actual == expected) {
        std::printf(
            "PARITY OK: set_class_table.json reproduced byte-for-byte "
            "(4096 rows, %zu bytes, computed+emitted in %lld us)\n",
            actual.size(), static_cast<long long>(elapsed));
        return 0;
    }

    // Locate the first differing byte and report the surrounding row.
    const std::size_t limit = std::min(actual.size(), expected.size());
    std::size_t diff = 0;
    while (diff < limit && actual[diff] == expected[diff]) ++diff;
    std::fprintf(stderr,
                 "PARITY FAIL: first difference at byte %zu "
                 "(ours %zu bytes, engine %zu bytes)\n",
                 diff, actual.size(), expected.size());
    const std::size_t from = diff > 120 ? diff - 120 : 0;
    std::fprintf(stderr, "engine: ...%s...\n",
                 expected.substr(from, 240).c_str());
    std::fprintf(stderr, "ours:   ...%s...\n",
                 actual.substr(from, 240).c_str());
    return 1;
}
