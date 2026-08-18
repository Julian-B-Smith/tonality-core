# Agent Charter — tonality-core

Everything above §Domain is the invariant harness layer. Do not edit it
per-project. Project-specific facts live in §Domain and in ROADMAP.md.

## Truth contract

- **ROADMAP.md is the single source of truth.** Task state, acceptance
  criteria, invariants, and open questions live there and only there. If the
  conversation and ROADMAP.md disagree, ROADMAP.md wins; if ROADMAP.md is
  wrong, fixing it is the first task.
- **DECISIONS.md is the append-only decision log** (kit 2.0.0). A ratified
  choice and its *why* land there, never in ROADMAP; supersede a decision
  with a new one that cites it, never edit it.
- **Passing ≠ done.** Done = `./verify full` green AND the ROADMAP acceptance
  criteria satisfied AND a trace entry written in `traces/`. Never collapse
  these into each other.
- **Grounded refusal is a success class.** "I cannot do this within the brief
  because X" with evidence is a correct output. Guessing to appear productive
  is a failure.
- **Reduce, never invent.** Prefer deleting code, tightening a contract, or
  reusing an existing mechanism over adding a new one. Every new abstraction
  must displace at least as much complexity as it introduces.

## Provenance

- Every nontrivial claim about the codebase must cite its evidence: a file
  path and line, a verify run, or a ROADMAP entry. No provenance → phrase it
  as a hypothesis, not a fact.
- Every merged change gets an entry in `traces/`: what changed, why, evidence
  consulted, verify result + git hash.

## Oracle discipline

- Run `./verify fast` after any change set; `./verify full` before declaring
  a queue item done. Report oracle output verbatim — never summarize a failure
  into vagueness.
- A red oracle halts forward work. Fix or revert; do not stack changes on red.
- Never weaken a gate (skip a test, relax a tolerance, drop a row from the
  parity sweep) without an explicit human decision recorded in DECISIONS.md.
- **Measured is not guaranteed.** State which one a claim is. Anything with a
  nanosecond attached is measured, on one host, and does not transfer.

## Human gates

Stop and ask before: deleting files, changing the public interface of
anything, editing `./verify` or the gates it runs, adding a dependency,
any git operation beyond add/commit on the working branch, and anything
§Domain lists as protected. **Pushes are the human's.**


<!-- kit:mailbox:2.1.0 — appended by /retrofit; kit-owned section -->
## Mailbox

This repo's topology is the reason the usual sentence needs two halves: we are
a **consumer**, so our inbound channel lives in the provider's tree.

- **Briefs to us land in `~/Documents/Tonality/integrations/tonality-core/`.**
  That is the provider's intake slot for this consumer, and it is where every
  notice, response and ratification in this repo's history has arrived. Reading
  it is a deliberate act at session start — nothing in our own tree changes
  when something lands there.
- **If we ever gain a direct consumer, `integrations/` in THIS repo is the
  only place their briefs land.** FOUNDATIONS reaches us today by relay
  *through* Tonality (`integrations/foundations/` there); if that ever becomes
  a direct exchange, it gets a slot here and this clause governs it.
- **Responses to OUR briefs live in the PROVIDER's tree**, not here, and must
  be pulled and read deliberately. An answered thread and an ignored one look
  identical from our side until someone goes and looks.
- **Other repos' exchanges may be READ freely, but never ACTED on** and never
  raised to the human as ours. Reading X↔Y is fine and often useful; owning it
  is not. If an exchange between other repos genuinely concerns us, the
  response is to file our own brief — not to answer theirs.
- **Writes stay home.** We may write into our own slot in the provider's tree
  (the mailbox exception) and nowhere else in it, and we leave what we file
  UNCOMMITTED — committing into Tonality is its residents' act.
<!-- /kit:mailbox:2.1.0 -->

---

## §Domain — tonality-core

**What this is.** The native C++ core of Tonality — the performance /
generative / embedded half of a dual implementation (Tonality Decision 10).
The pure-Python engine remains a fully-functional peer and **the spec's
source of truth**; this repo never re-derives correctness, it *reproduces
fixtures* exported from the engine. See README.md.

**Stack & entrypoints.** Header-only C++20 under `include/tonality/`
(no dependencies). CMake + Unix Makefiles; optional pybind11 module under
`bindings/`. Tests are the parity harness under `tests/`, driven by ctest.
`./verify fast` = gates + fixture PIN hashes + ctest if a build tree exists;
`./verify full` = configure + Release build + the whole parity harness.

**Domain invariants** (never negotiated in conversation):
- **Python is the spec.** A disagreement between implementations is a bug
  HERE until the conformance golden says otherwise. Goldens and pins
  regenerate only on the Tonality side.
- **Parity is BYTE parity**, not approximate agreement — 4096 rows × 16
  fields, plus every field of the `set_class_info` conformance case. An
  unrecognized case field fails the harness, so engine growth is loud.
- **Byte parity is exact-arithmetic parity.** `-ffp-contract=off` is
  load-bearing; `cmul`/`cpowu` encode CPython's compiled-in FMA fusion
  explicitly, float reductions replicate Neumaier-compensated `sum()`, and
  `pow` is forced through libm. Matching libm beats being mathematically
  nicer. Do not "simplify" any of this.
- **Byte parity is platform-specific.** macOS 15 arm64 is canonical because
  that is where the fixtures were generated; Linux is a tolerance probe.
  Phases are compared as coefficients — a vanishing component's phase is
  information-free.
- **Port by stability.** Nothing beyond the exported `SET_CLASS_TABLE_FIELDS`,
  and nothing past the **Phase 6 fence** regardless. Phase 6 renegotiates
  "the mask is the key", so porting the layers above the identity substrate
  now would mean porting them twice. A small surface here is the intended
  state of a stability-gated port, not a backlog.
- **The measure of currency is the PIN being green**, not the tool count.
- **Never edit `mts/`.** Engine-side asks go as briefs to the mailbox above.

**Protected paths.** `fixtures/` and `fixtures/PIN.json` (engine-owned —
refresh only via `tools/refresh_fixtures.sh`); the arithmetic-parity code in
`include/tonality/{dft,chirality}.hpp`; `verify`, `.kit/`, and this charter.

**Real-time surface.** README.md §"The real-time surface" carries the tier
list and is the answer of record to FOUNDATIONS `foundations-001`. Two
constraints are easy to break by accident: `py_round_10`'s `snprintf`/`strtod`
is parity, not sloppiness; and an RT-certified DFT and a byte-parity DFT are
not necessarily the same code.

**Verify targets.** fast: milliseconds-to-seconds (gates, PIN hashes, ctest
when a build tree exists). full: minutes (configure + Release build + parity).
CI runs `fast` as the harness job and the parity matrix separately.
