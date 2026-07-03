// Python-repr-compatible JSON number formatting.
//
// The engine writes set_class_table.json with json.dumps(...), whose float
// serialization is CPython's repr: the shortest digit string that round-trips,
// rendered fixed when the decimal exponent E satisfies -4 <= E < 16 and
// scientific otherwise (mantissa 'd[.ddd]', 'e', explicit sign, >=2 exponent
// digits; integral fixed values get a trailing '.0'). std::to_chars supplies
// the same shortest digits; this reformats them under Python's placement rules.
#pragma once

#include <charconv>
#include <string>

namespace tonality {

inline void append_python_float_repr(std::string& out, double value) {
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::scientific);
    const char* p = buf;
    const char* end = res.ptr;

    if (*p == '-') {
        out += '-';
        ++p;
    }

    std::string digits;  // significant digits, no decimal point
    for (; p < end && *p != 'e'; ++p) {
        if (*p != '.') digits += *p;
    }
    int exponent = 0;  // decimal exponent of the FIRST digit (value = d.dd * 10^E)
    if (p < end && *p == 'e') {
        // to_chars output is NOT null-terminated — bound the parse by `end`.
        exponent = std::stoi(std::string(p + 1, end));
    }

    // Strip trailing zeros to_chars may have kept (shortest form usually has
    // none, but "0e+00" for zero does).
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();

    if (digits == "0") {  // ±0.0
        out += "0.0";
        return;
    }

    const int n = static_cast<int>(digits.size());
    if (-4 <= exponent && exponent < 16) {  // fixed notation
        if (exponent >= n - 1) {  // integral: pad zeros, add ".0"
            out += digits;
            out.append(static_cast<std::size_t>(exponent - (n - 1)), '0');
            out += ".0";
        } else if (exponent >= 0) {  // point inside the digits
            out.append(digits, 0, static_cast<std::size_t>(exponent + 1));
            out += '.';
            out.append(digits, static_cast<std::size_t>(exponent + 1), std::string::npos);
        } else {  // leading "0.000…"
            out += "0.";
            out.append(static_cast<std::size_t>(-exponent - 1), '0');
            out += digits;
        }
    } else {  // scientific notation
        out += digits[0];
        if (n > 1) {
            out += '.';
            out.append(digits, 1, std::string::npos);
        }
        out += 'e';
        out += exponent < 0 ? '-' : '+';
        const int abs_exp = exponent < 0 ? -exponent : exponent;
        if (abs_exp < 10) out += '0';
        out += std::to_string(abs_exp);
    }
}

}  // namespace tonality
