"""Parity harness for the pybind11 fast path.

Two modes, because byte-for-byte float parity is a property of the *fixture-
generating platform*, not of the port (glibc and Apple libm differ by ~1 ulp on
transcendental DFT terms, and shortest-repr JSON turns that ulp into different
bytes — see .github/workflows/parity.yml):

**Byte-exact mode (default)** — the canonical platform, macOS. The bindings must
reproduce the engine's export exactly, through both of their surfaces:

1. emit_table_json() — byte-identical to the vendored set_class_table.json.
2. set_class_row(mask) — dict-equal to the parsed fixture row for every one of
   the 4096 masks (Python `==`: exact ints, exact float bit patterns via JSON
   round-trip; the engine's mask-0 int-vs-float quirk compares equal by design).
3. The set_class_info golden case, slice-1 fields, within the golden tolerances.

**Tolerance mode (`--tolerance`)** — the portable leg, Linux. Same 4096 rows,
same field-by-field coverage, but floats are compared within the goldens'
tolerances (rel 1e-9 / abs 1e-12) instead of by bit pattern, and the byte-
identical emit check is skipped. Integer, list-of-int and null fields stay
EXACT in both modes: the tolerance is for libm's last ulp, never for the
combinatorics, and an integer field diverging across platforms would be a real
port bug, not a rounding artifact. This is the same ints-exact/floats-toleranced
rule the engine's own test_port_pin.py applies. Ratified as the cross-platform
parity contract in the Tonality repo's
integrations/tonality-core/ratify-ci-required.md (2026-07-13).

Usage: parity_bindings.py <fixtures_dir> <module_dir> [--tolerance]
"""

from __future__ import annotations

import json
import math
import sys
import time
from pathlib import Path

argv = [a for a in sys.argv[1:] if a != "--tolerance"]
TOLERANCE_MODE = "--tolerance" in sys.argv
FIXTURES = Path(argv[0])
sys.path.insert(0, argv[1])

import tonality_core as tc  # noqa: E402

CASE_FIELDS = {
    "mask", "normal_order", "prime_form", "prime_form_mask", "dft_magnitudes",
    "z_partner_prime_form", "complement_prime_form", "rotational_period",
    "dft_phases", "trichord_chirality", "general_chirality", "chirality_sign",
    "chirality", "reflection_residual",
}
FLOAT_LIST_FIELDS = {"dft_magnitudes", "dft_phases"}
FLOAT_FIELDS = {"general_chirality", "chirality", "reflection_residual"}

failures = 0


def check(condition: bool, message: str) -> None:
    global failures
    if not condition:
        print(f"FAIL: {message}", file=sys.stderr)
        failures += 1


# The goldens own the tolerances; the port never picks its own.
golden = json.loads((FIXTURES / "conformance.json").read_text(encoding="utf-8"))
rel, abs_tol = golden["float_rel_tol"], golden["float_abs_tol"]


def close(expected: object, actual: object) -> bool:
    """Float compare under the golden tolerances; int/None fields stay exact.

    `math.isclose` accepts the mask-0 rows' int-typed magnitudes as-is, and
    treats -0.0 and 0.0 as equal — the engine emits both (max(x, 0.0) passes
    -0.0 through), and their difference is not a parity signal.
    """
    if isinstance(expected, (int, float)) and isinstance(actual, (int, float)):
        return math.isclose(expected, actual, rel_tol=rel, abs_tol=abs_tol)
    return expected == actual


def field_matches(field: str, expected: object, actual: object) -> bool:
    if field in FLOAT_LIST_FIELDS:
        return (isinstance(actual, list) and len(actual) == len(expected)
                and all(close(e, a) for e, a in zip(expected, actual)))
    if field in FLOAT_FIELDS:
        return close(expected, actual)
    return actual == expected  # ints, lists of ints, nulls — exact, always


table_text = (FIXTURES / "set_class_table.json").read_text(encoding="utf-8")
rows = json.loads(table_text)

if not TOLERANCE_MODE:
    # 1. byte parity through the bindings (canonical platform only)
    emitted = tc.emit_table_json()
    check(emitted == table_text,
          f"emit_table_json() differs from fixture "
          f"({len(emitted)} vs {len(table_text)} bytes)")

    # 2. per-row dict equality, all 4096 masks
    start = time.perf_counter()
    mismatches = [mask for mask, expected in enumerate(rows)
                  if tc.set_class_row(mask) != expected]
    elapsed = time.perf_counter() - start
    check(not mismatches, f"row mismatch at masks {mismatches[:10]}")
else:
    # 2'. per-row, per-field comparison under the golden tolerances. Reported
    # per field rather than per row: on a portability leg, WHICH field diverged
    # is the whole diagnostic (a float field = libm; an int field = a port bug).
    start = time.perf_counter()
    diffs: list[str] = []
    for mask, expected in enumerate(rows):
        actual = tc.set_class_row(mask)
        if set(actual) != set(expected):
            diffs.append(f"mask {mask}: field set differs "
                         f"{sorted(set(actual) ^ set(expected))}")
            continue
        diffs.extend(
            f"mask {mask}.{field}: {expected[field]!r} vs {actual[field]!r}"
            for field in expected
            if not field_matches(field, expected[field], actual[field])
        )
    elapsed = time.perf_counter() - start
    check(not diffs, f"{len(diffs)} field mismatch(es) beyond tolerance; "
                     f"first 10:\n  " + "\n  ".join(diffs[:10]))

# 3. the golden set_class_info case — every field, none deferred
case = next(c for c in golden["cases"] if c["tool"] == "set_class_info")
mask = tc.mask_from_pcs(case["kwargs"]["pcs"])
row = tc.set_class_row(mask)
check(set(case["result"]) == CASE_FIELDS,
      f"conformance case fields drifted: {sorted(set(case['result']) ^ CASE_FIELDS)}")
for field in sorted(CASE_FIELDS & set(case["result"])):
    check(field_matches(field, case["result"][field], row[field]),
          f"golden {field}: {case['result'][field]} vs {row[field]}")

if failures:
    print(f"{failures} check(s) failed", file=sys.stderr)
    sys.exit(1)

if TOLERANCE_MODE:
    print(f"PARITY OK (bindings, tolerance): 4096/4096 rows match within "
          f"rel {rel} / abs {abs_tol}, ints exact "
          f"({elapsed * 1000:.1f} ms through Python) + golden case reproduced")
else:
    print(f"PARITY OK (bindings): byte-identical emit + 4096/4096 rows dict-equal "
          f"({elapsed * 1000:.1f} ms through Python) + golden case reproduced")
