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

- **Slice 1 (identity layer):** regenerate `set_class_table.json` from this
  core and diff **byte-for-byte** against the vendored export (4096 rows);
  reproduce the slice-1 fields of the `set_class_info` conformance case
  within the golden tolerances (rel 1e-9 / abs 1e-12).

Slice-1b fields (`dft_phases`, `chirality`, `chirality_sign`,
`general_chirality`, `trichord_chirality`) are **deliberately not ported yet**
(held until they join the engine's export table — PORT.md adopted default 4);
the conformance runner reports them as deferred, not failed.

## Layout

```
include/tonality/   header-only core: bitmask.hpp, dft.hpp, setclass.hpp,
                    table.hpp (row compute + Python-json-identical emit)
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
