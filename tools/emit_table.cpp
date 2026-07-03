// Regenerate set_class_table.json from this core (the C++ side of the
// byte-for-byte acceptance diff). Writes to stdout, or to the path given as
// the first argument.
#include <cstdio>
#include <fstream>
#include <string>

#include "tonality/table.hpp"

int main(int argc, char** argv) {
    const std::string json = tonality::emit_table_json();
    if (argc > 1) {
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 2;
        }
        out << json;
        std::fprintf(stderr, "wrote %s (%zu bytes)\n", argv[1], json.size());
    } else {
        std::fwrite(json.data(), 1, json.size(), stdout);
    }
    return 0;
}
