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

## Harness

Retrofit to harness kit **2.4.1** on 2026-08-18. The files a fresh session
should read first:

- [CLAUDE.md](CLAUDE.md) — agent charter; §Domain carries the invariants that
  are never renegotiated in conversation (and the mailbox, which lives in the
  *provider's* tree, not here).
- [ROADMAP.md](ROADMAP.md) — direction of record for this repo, phase-gated.
  Defers to Tonality's `CPP_PORT.md` / `ROADMAP.md` Phase 8 on port ordering.
- [DECISIONS.md](DECISIONS.md) — append-only; why each ratified choice was made.
- [INDEX.md](INDEX.md) → [LIBRARY.md](LIBRARY.md) — hard-won lessons, retrieved
  by trigger rather than read end-to-end.
- `./verify fast` — gates + fixture hashes against `fixtures/PIN.json` + ctest
  when a build tree exists. `./verify full` is the parity definition of done
  (configure + Release build + the whole harness). Kit-owned gate code is
  vendored and sha256-pinned in `.kit/`; do not edit it, re-sync it.

*Last verified 2026-08-18: `./verify full` green, parity 4/4 against engine
PIN `0c62809`.*

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

## The real-time surface

Separate from parity, and asked for by name: which of this core may be called
from an audio thread. *Measured 2026-08-09 against engine PIN `0c62809`; full
evidence and method in `integrations/tonality-core/response-foundations-rt-questions.md`
(Tonality repo), exchange `foundations-001`.*

**The boundary is between transports, not between fields.** The input domain is
a 12-bit mask — 4096 possible inputs — so every field here is available two
ways, and they land on opposite sides of the line: computing `chirality(mask)`
costs up to 17.25 µs, reading it from a frozen 4096-row table costs 1.46 ns.
Freezing the table (416 KiB at float32, 672 KiB at double, ~77 ms to build off
the RT thread) makes the **entire** surface real-time — and it is not a cache,
because with 4096 total inputs the miss case does not exist.

For live computation:

| tier | what | status |
|---|---|---|
| **A** | the integer identity layer — `cardinality`, `is_subset`, `rotate_mask`, `invert_mask`, `complement_mask`, `pcs_from_mask`, `interval_vector`, `rotational_period`, `normal_order`, `prime_form[_mask]`, `trichord_chirality`, and `ZTableHandle::partner` | **RT.** No external calls beyond the stack canary, zero allocations, loop counts fixed by the 12-pc universe. 0.3–140 ns. |
| **B** | `z_partner_mask` (free function) | Guarded static: first call builds the Z-table (~0.6 ms). Use `ZTableHandle` from RT code; this stays for the table generator and the bindings. |
| **C** | `dft_components`, `dft_magnitudes`, `dft_phases`, `chirality_sign` | Allocation-free and lock-free, but they call libm. RT only if *your* libm is — we do not ship it, so we do not certify it. |
| **D** | `py_round_10`, `general_chirality`, `reflection_residual`, `chirality`, `compute_row`, `emit_table_json` | **Not RT.** `snprintf`/`strtod` (locale), a 420-iteration minimizer, and a 421× data-dependent spread in `chirality`. Freeze these. |

Two constraints that are load-bearing here and easy to break by accident:

- **`py_round_10`'s `snprintf`/`strtod` round-trip is parity, not sloppiness** —
  it reproduces CPython's `round(x, 10)`. It keeps Tier D in Tier D, and it
  does not get "optimized away".
- **An RT-certified DFT and a byte-parity DFT are not necessarily the same
  code.** Tier C is only libm-bound, so vendoring a polynomial `sin`/`cos`
  would make it portable-RT — and would break the parity contract this repo
  exists to hold. The frozen table is the way out: generated on the parity
  platform, parity-exact everywhere.

`chirality_slices()` is a `constexpr std::array` and `ZTableHandle` exists
*because* of this analysis — the first cost `chirality_sign` an allocation, a
guard and an exception path; the second turns "warm up before the audio thread
starts" from a comment into a precondition the type system holds. Neither
changed a single byte of the export.

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

This adds two more ctests over the same 4096 rows: `parity_bindings` (byte-identical
`emit_table_json()` plus dict-equality of every `set_class_row()` result against
the fixture) and `parity_bindings_tolerance` (the portable comparison CI runs off
the fixture-generating platform — floats within the goldens' tolerances, ints
exact; see [Continuous integration](#continuous-integration)).

## Continuous integration

`.github/workflows/parity.yml` runs the parity harness on every push to `main`
and every pull request, across a **`ubuntu-latest` + `macos-15` matrix**. It
rebuilds the core from source (Release, pybind11 fast path on) and reruns the
ctests — `parity_table`, `parity_conformance`, `parity_bindings`, and
`parity_bindings_tolerance` — that the watcher PRs previously ran by hand. No new
*assertions* about the engine; the CI automates the definition of done, and the
tolerance mode re-checks the same 4096 rows under a portable comparison.

Cross-platform is the point, not decoration — but with a boundary the first CI
run made concrete. **Byte-for-byte float parity is platform-specific.** The
fixtures were exported by the CPython engine on macOS, and glibc vs Apple libm
disagree by ~1 ulp on the transcendental DFT terms (the first run saw a
`dft_magnitude` of `1.0` on macOS vs `0.9999999999999999` on Linux); shortest-repr
JSON turns that ulp into different bytes. So the matrix splits:

- **`macos-15` — canonical:** the full harness, including the byte-exact
  `parity_table` and `parity_bindings`. macOS is the platform the fixtures encode.
  The image is **pinned** rather than `macos-latest`, so a rolling image can't red
  the canonical leg on a libm change that is not a bug — and it is pinned to
  **macOS 15 arm64 specifically, because that is where the fixtures are
  generated**. The version, not just the arch, is the binding constraint:
  pinning to `macos-14` (also arm64) turned this leg red, its libm giving
  `reflection_residual` `+0.0` where the fixture has `-0.0` — one signed zero,
  which shortest-repr JSON renders as different bytes. Regenerate the fixtures on
  a different macOS and this pin moves with them.
- **`ubuntu-latest` — portability probe:** builds from source (proves the headers
  compile under GCC and the algorithm ports) and checks values within tolerance
  (rel 1e-9 / abs 1e-12) rather than bytes — the same float tolerancing the
  engine's own `test_port_pin.py` applies — across **all 4096 rows**
  (`parity_bindings_tolerance`) plus the `set_class_info` case
  (`parity_conformance`). Integer, list-of-int and null fields are compared
  **exactly on both legs**: the tolerance is for libm's last ulp, never for the
  combinatorics, so an integer diverging across platforms is a real port bug and
  still fails. A real numeric regression (> tolerance) fails here on either OS; a
  sub-ulp libm difference correctly does not. The runner stays rolling on purpose
  — a portability probe wants toolchain drift.

  One field is compared indirectly: **`dft_phases` via the complex coefficient**
  it and `dft_magnitudes` describe together. A phase is a polar coordinate, and
  where a component's magnitude vanishes its phase is the `atan2` of two
  rounding-noise terms — information-free. Linux and macOS disagree by up to
  **0.53 rad** on such phases, every one of them at a magnitude of ~1e-16, so
  comparing phases directly would assert an invariant that does not hold. The
  coefficient comparison keeps full sensitivity where a component is significant
  (a 1e-6 rad error on a unit-magnitude component still fails), is automatically
  correct at the ±π branch cut, and treats a vanishing component as the zero it
  is.

`fail-fast` is off so each OS reports independently. This closes the loop
`port/PORT.md` promised — engine drift fails Tonality's build, port drift fails
this build.

The cross-platform split above is the **ratified** parity contract: the engine's
dev loop accepted it in `integrations/tonality-core/ratify-ci-required.md` (in the
Tonality repo), answering the `notice-ci-required.md` ask and this repo's
`response-ci-required.md` finding — *byte-exact on the fixture-generating platform
(macOS), values-within-tolerance everywhere.* Both refinements agreed in that
ratification are now **applied** (they are the workflow described above):

- **Pinned macOS runner** — rather than `macos-latest`, so a rolling GitHub image
  can't turn the *canonical* byte-exact leg red on a non-bug (the very failure the
  split exists to prevent, reintroduced through the runner label). Applied as
  `macos-15`, not the `macos-14` the ratification suggested: CI measured that
  macos-14's libm flips a signed zero against the fixtures, which confirms the
  ratification's hazard rather than contradicting it — the image version is
  load-bearing, so the pin has to track the *generating* platform (macOS 15
  arm64), not merely a fixed label.
- **All-rows Linux tolerance mode** — `parity_bindings_tolerance` checks all 4096
  table rows within tolerance on Linux, not just the single `set_class_info`
  conformance case, strengthening the portability probe at no cost to the macOS
  byte-exact guarantee. With one refinement the ratification could not have
  foreseen: phases are compared as coefficients (above), because the naive
  all-rows phase comparison asserts a false invariant wherever a magnitude
  vanishes.

The watcher's refresh PRs land only on green CI; each PR's acceptance block cites
the CI run rather than a single local build. **Branch protection on `main`**
(require `parity (macos-15)` and `parity (ubuntu-latest)` green before merge) is a
repo setting the maintainer enables in GitHub — the workflow provides the checks;
the gate is set once there. Until it is on, CI reports but does not gate.

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
