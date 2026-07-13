# tonality-core

Native C++ core of [Tonality](https://github.com/Lifted-Truck/Tonality) — the
performance / generative / embedded main of the dual implementation
(Tonality Decision 10, revised 2026-06-29). The pure-Python engine remains a
fully-functional peer and **the spec's source of truth**; this repo never
re-derives correctness — it *reproduces fixtures* exported from the engine.

Contract documents (in the Tonality repo — read them first):

- `CPP_PORT.md` — what to build, slice by slice, and each slice's acceptance.
- `port/PORT.md` — the two-thread accountability protocol this repo lives under.
- `ROADMAP.md` Phase 8 — direction of record.

## Scope — deliberately the frozen kernel, not the whole engine

This repo ports **only the identity substrate**: the 4096-row set-class table
(`SET_CLASS_TABLE_FIELDS` — prime form, normal order, interval vector, DFT
magnitude/phase, Z-partner, complement, rotational period, chirality) and its
`set_class_info` conformance case. That is a small fraction of the engine's
surface **by design, not by lag.** Under *port-by-stability* (`port/PORT.md`),
only the frozen core is ever dual-implemented, and nothing is ported past the
**Phase 6 fence** — Phase 6 renegotiates "the mask is the key," so porting the
layers above the identity substrate now would mean porting them twice. The
analysis, temporal, rules, search, and pattern layers of the engine stay
Python-only until they freeze.

So "tonality-core reproduces N of the engine's M tools" is the *intended* state of
a stability-gated port, not a backlog. **The measure of whether this repo is
current is the pin being green — that the ported surface still matches the
engine byte-for-byte — not the tool count.** When the engine grows a tool above
the identity substrate, that is *expected* to leave this repo unchanged; only a
change to the exported set-class table (a new export field, an arithmetic change)
is a port-relevant event, and it arrives as a brief on the
`integrations/tonality-core/` channel.

## The parity contract

`fixtures/tonality/` vendors the engine's exported artifacts
(`set_class_table.json`, `manifest.json`, `bundle.json`,
`conformance.json`), pinned to an engine commit in `fixtures/PIN.json`.
A build of this core is **correct iff it reproduces them**:

- **Slices 1 + 1b (identity layer + chirality/DFT-phase family, export.2):**
  regenerate `set_class_table.json` from this core and diff **byte-for-byte**
  against the vendored export (4096 rows × 16 fields); reproduce **every**
  field of the `set_class_info` conformance case within the golden tolerances
  (rel 1e-9 / abs 1e-12) — nothing deferred, and an unrecognized case field
  fails the harness so engine surface growth is loud.

Byte parity is exact-arithmetic parity: this repo compiles with
`-ffp-contract=off` (interpreter-level Python never fuses), while CPython's
*C-compiled* complex primitives DO fuse on this platform — so `cmul`/`cpowu`
encode that fusion explicitly with `std::fma`, float reductions replicate
CPython's Neumaier-compensated `sum()`, and `pow` is forced through libm
(LLVM's folded `pow(x,2)→x*x` is 1 ulp *better* than Apple's libm in places —
parity means matching libm, not the mathematically nicer answer).

## Layout

```
include/tonality/   header-only core: bitmask.hpp, dft.hpp, setclass.hpp,
                    table.hpp (row compute + Python-json-identical emit)
bindings/           optional pybind11 fast path (module `tonality_core`) —
                    an addition for Python consumers, never a replacement
                    for the pure-Python engine (Decision 10)
tools/              emit_table.cpp (regenerate the table from this core),
                    refresh_fixtures.sh (pull-side fixture refresh — run at
                    the start of every port work session)
tests/              the parity harness — the definition of done
fixtures/tonality/  vendored engine exports + PIN.json
```

## Build & run parity (this machine: Command Line Tools only, no Ninja/Xcode)

```bash
cmake -S ~/Documents/tonality-core -B ~/Documents/tonality-core/build-release \
      -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build ~/Documents/tonality-core/build-release -j"$(sysctl -n hw.ncpu)"
ctest --test-dir ~/Documents/tonality-core/build-release --output-on-failure
```

Always pass **absolute** build paths (agent shells reset cwd between calls; a
relative build silently builds nothing). Unix Makefiles is single-config — use
separate `build-debug`/`build-release` dirs. All perf claims from Release only.

To build the Python fast path too, add (pointing at an interpreter with
`pybind11` installed — e.g. the Tonality venv):

```bash
-DTONALITY_BUILD_PYTHON=ON \
-DPython_EXECUTABLE=~/Documents/Tonality/.venv/bin/python3.13
```

This adds a third ctest (`parity_bindings`): byte-identical `emit_table_json()`
plus dict-equality of all 4096 `set_class_row()` results against the fixture.

## Fences (mirror of port/PORT.md)

- Python is the spec. A disagreement between implementations is a bug **here**
  until the conformance golden says otherwise; goldens and pins regenerate
  only on the Tonality side.
- Port by stability: nothing beyond the exported `SET_CLASS_TABLE_FIELDS`
  until slice 1 is byte-identical; nothing past the Phase 6 fence regardless.
- Engine-side asks (e.g. new export fields) go as briefs to
  `integrations/tonality-core/` in the Tonality repo — never edit `mts/`.
- Every PR carries an acceptance block: "reproduces these N fixture rows /
  M conformance cases against engine commit `<sha>`".
