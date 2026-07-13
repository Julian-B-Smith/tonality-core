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

## Continuous integration

`.github/workflows/parity.yml` runs the parity harness on every push to `main`
and every pull request, across a **`ubuntu-latest` + `macos-latest` matrix**. It
rebuilds the core from source (Release, pybind11 fast path on) and reruns the
three ctests — `parity_table`, `parity_conformance`, `parity_bindings` — that the
watcher PRs previously ran by hand. No new tests; the CI just automates the
definition of done.

Cross-platform is the point, not decoration: the pin-determinism incident proved
a single-machine parity claim hides ULP / libm / platform drift until someone
else builds it. Byte-for-byte float parity is reproduced against fixtures the
engine exported on macOS, so the `ubuntu-latest` leg is a real probe of that
claim (`fail-fast` is off so each OS reports independently). This closes the loop
`port/PORT.md` already promised — engine drift fails Tonality's build, port drift
fails this build. See `integrations/tonality-core/notice-ci-required.md` (in the
Tonality repo) for the ask this satisfies.

The watcher's refresh PRs land only on green CI; each PR's acceptance block cites
the CI run rather than a single local build. **Branch protection on `main`**
(require the parity checks green before merge) is a repo setting the maintainer
enables in GitHub — the workflow provides the checks; the gate is set once there.

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
