"""Parity harness for the pybind11 fast path.

The bindings must reproduce the engine's exported table exactly, through both
of their surfaces:

1. emit_table_json() — byte-identical to the vendored set_class_table.json.
2. set_class_row(mask) — dict-equal to the parsed fixture row for every one of
   the 4096 masks (Python `==`: exact ints, exact float bit patterns via JSON
   round-trip; the engine's mask-0 int-vs-float quirk compares equal by design).
3. The set_class_info golden case, slice-1 fields, within the golden tolerances.

Usage: parity_bindings.py <fixtures_dir> <module_dir>
"""

from __future__ import annotations

import json
import math
import sys
import time
from pathlib import Path

FIXTURES = Path(sys.argv[1])
sys.path.insert(0, sys.argv[2])

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


# 1. byte parity through the bindings
table_text = (FIXTURES / "set_class_table.json").read_text(encoding="utf-8")
emitted = tc.emit_table_json()
check(emitted == table_text,
      f"emit_table_json() differs from fixture "
      f"({len(emitted)} vs {len(table_text)} bytes)")

# 2. per-row dict equality, all 4096 masks
rows = json.loads(table_text)
start = time.perf_counter()
mismatches = [mask for mask, expected in enumerate(rows)
              if tc.set_class_row(mask) != expected]
elapsed = time.perf_counter() - start
check(not mismatches, f"row mismatch at masks {mismatches[:10]}")

# 3. the golden set_class_info case — every field, none deferred
golden = json.loads((FIXTURES / "conformance.json").read_text(encoding="utf-8"))
case = next(c for c in golden["cases"] if c["tool"] == "set_class_info")
mask = tc.mask_from_pcs(case["kwargs"]["pcs"])
row = tc.set_class_row(mask)
rel, abs_tol = golden["float_rel_tol"], golden["float_abs_tol"]
check(set(case["result"]) == CASE_FIELDS,
      f"conformance case fields drifted: {sorted(set(case['result']) ^ CASE_FIELDS)}")
for field in sorted(CASE_FIELDS & set(case["result"])):
    expected = case["result"][field]
    actual = row[field]
    if field in FLOAT_LIST_FIELDS:
        check(all(math.isclose(e, a, rel_tol=rel, abs_tol=abs_tol)
                  for e, a in zip(expected, actual)),
              f"golden {field}: {expected} vs {actual}")
    elif field in FLOAT_FIELDS:
        check(math.isclose(expected, actual, rel_tol=rel, abs_tol=abs_tol),
              f"golden {field}: {expected} vs {actual}")
    else:
        check(actual == expected, f"golden {field}: {expected} vs {actual}")

if failures:
    print(f"{failures} check(s) failed", file=sys.stderr)
    sys.exit(1)

print(f"PARITY OK (bindings): byte-identical emit + 4096/4096 rows dict-equal "
      f"({elapsed * 1000:.1f} ms through Python) + golden case reproduced")
