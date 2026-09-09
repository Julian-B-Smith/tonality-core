# tonality-core — ROADMAP

> **Single source of truth for this project's direction.** Phase gates are
> never weakened to pass. Cross-project sequencing lives in Tonality's
> `ROADMAP.md` Phase 8 and `CPP_PORT.md`; this file defers to them on port
> ordering and never contradicts them. The *why* behind ratified choices lives
> in [DECISIONS.md](DECISIONS.md), not here.

## Prior-art bookends (autonomous Decision 30)

- **Phase 0 — Prior-art landscape: NOT APPLICABLE, by ruling.** This repo did
  not begin at a blank design space: it is the second implementation of an
  engine whose domain modelling, algorithm choice and prior-art position were
  settled upstream in Tonality (Decision 10, revised 2026-06-29). A landscape
  pass here would re-survey a decision this repo does not own and cannot act
  on. Rationale recorded as DECISIONS #2 — if the port ever originates design
  rather than reproducing it, this bookend comes back.
- **Pre-ship prior-art & IP re-scan: OPEN, gates any public release.** The
  set-class arithmetic is long-established music theory, but a re-scan is
  still owed before this core ships inside anything commercial (a plugin
  especially). Findings land dated and cited in `docs/prior-art.md`.

## Build sequence (phase-gated)

- **Slice 1 — identity layer.** Mask ops, Rahn normal order / prime form,
  interval vector, DFT magnitudes, Z-partner, complement, rotational period;
  byte-identical `set_class_table.json`. *Gate: 4096 rows byte-for-byte
  against the vendored export.* **CLOSED.**
- **Slice 1b — chirality / DFT-phase family (`export.2`).** `dft_phases`,
  `trichord_chirality`, `general_chirality`, `chirality_sign`, `chirality`,
  `reflection_residual`. *Gate: byte-for-byte table + every field of the
  `set_class_info` conformance case within golden tolerances (rel 1e-9 /
  abs 1e-12), nothing deferred.* **CLOSED.**
- **CI — the parity loop's port half.** *Gate: `macos-15` byte-exact leg +
  `ubuntu-latest` all-rows tolerance leg, both green, fail-fast off.*
  **CLOSED** — ratified in `ratify-ci-required.md`; both agreed refinements
  applied (pinned image, all-rows Linux mode). Branch protection on `main`
  is a repo setting the maintainer enables; until it is on, CI reports but
  does not gate.
- **RT surface — certification for FOUNDATIONS (`foundations-001`).**
  *Gate: a measured tier list, allocation-trapped and symbol-audited, with the
  answer filed on the exchange channel.* **CLOSED 2026-08-10** (PR #11):
  `chirality_sign` moved Tier D→C (constexpr slice family; 7 allocations → 0),
  `ZTableHandle` makes the Z-table warm-up a type. Tier list of record lives
  in README.md §"The real-time surface".
- **Harness retrofit to kit 2.4.1.** *Gate: `currency.py` reads CURRENT with
  nothing missing, `./verify fast` green, gate proven to FIRE on a planted
  identity path.* **← current.**

## Standing state (not a phase)

**Pin currency.** The measure of whether this repo is current is the PIN being
green — that the ported surface still reproduces the engine's export
byte-for-byte — *not* the tool count. Engine growth ABOVE the identity
substrate is expected to leave this repo unchanged. Only a change to the
exported set-class table (a new field, an arithmetic change) is a
port-relevant event, and it arrives as a brief on the mailbox channel.
Current PIN: engine `0c62809`, `export.2`.

## Open questions

- **Live vs frozen, for FOUNDATIONS.** Their answer decides whether any Tier-C
  or Tier-D work is worth doing at all. Nothing here is blocked meanwhile.
  If they want live, two scoped tasks become real: an unrounded variant of
  `general_chirality` / `reflection_residual` (moves them off
  `snprintf`/`strtod`), and a decision on whose libm certifies Tier C.
- **`py_round_10` without `snprintf`/`strtod`.** `std::to_chars` covers the
  format half and is already used in `json_format.hpp`, but this toolchain's
  libc++ declares **no floating-point `std::from_chars`** at all (measured
  2026-08-18), so the parse half would mean vendoring a correctly-rounded
  decimal parser matching CPython's `_Py_dg_strtod` bit-for-bit. Scoped
  engineering, gated by the existing byte-parity harness — not a research
  question. Only worth starting if the live-vs-frozen question lands on live.
- **Key-estimate margin above the Phase 6 fence.** Correlating a 12-bin
  pitch-class distribution against 24 fixed profiles is a ~288-multiply-add
  dot product — nothing about key induction is intrinsically hostile to
  real-time. The blocker is governance, not computation: the profiles and
  normalization are unfrozen. Worth telling FOUNDATIONS so they leave a seam
  rather than architecting margin out permanently.
- **Branch protection on `main`** — the workflow supplies the checks; the gate
  is a GitHub setting only the maintainer can turn on.

## Debt

None currently red. The parity harness is 4/4 green at PIN `0c62809`; no test
is quarantined and no gate is relaxed.
