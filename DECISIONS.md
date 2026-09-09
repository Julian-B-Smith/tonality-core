# tonality-core — DECISIONS (append-only)

> The decision log. Append only: never edit or delete a prior decision;
> supersede it with a new one that cites it. ROADMAP.md holds task state,
> acceptance criteria, invariants and open questions; the *why* behind a
> ratified choice lives here.
>
> This log opens at the kit 2.4.1 retrofit (2026-08-18). Decisions 2–7 predate
> it and are recorded here because the evidence for each is unambiguous in the
> tree (README.md, `.github/workflows/parity.yml`, the exchange files in
> `~/Documents/Tonality/integrations/tonality-core/`, and merged PRs #9–#11).
> Where a decision was ratified on the Tonality side, that is stated — this
> repo records that it is BOUND by them, and does not claim to own them.

1. **Retrofit to kit 2.4.1** (2026-08-18). This repo carried no `kit_version`
   and read as `pre-2.0.0`: no charter, ROADMAP, DECISIONS, manifest,
   knowledge loop, traces, or `./verify`. Only CI and `.gitattributes` were
   present. Applied the full 2.0.0 baseline plus 2.1.0 (mailbox scope), and
   took kit mechanism through the vendored path (2.2.0/2.3.0/2.4.0 are
   answered by checksum once `.kit/` matches canonical, so no probe writes
   into this tree). Architecture rung **1 — single thread**, chosen not
   defaulted: 766 lines of header behind one byte-exact oracle over a
   4096-input domain; verification is instant and total and there are no
   parallelizable seams to hand a verifier.

2. **Prior-art Phase 0 is not applicable here** (2026-08-18, human-ratified
   during the retrofit). autonomous Decision 30 requires prior-art bookends on
   every ROADMAP. This repo did not begin at a blank design space — it is the
   second implementation of an engine whose domain modelling and prior-art
   position were settled upstream (Tonality Decision 10, revised 2026-06-29).
   A landscape pass here would re-survey a decision this repo neither owns nor
   can act on. The **pre-ship IP re-scan bookend stays open** and gates any
   public release. If the port ever originates design rather than reproducing
   it, Phase 0 comes back.

3. **Python is the spec; this repo reproduces, never re-derives**
   (Tonality Decision 10, revised 2026-06-29 — binding here, not owned here).
   A disagreement between implementations is a bug in this repo until the
   conformance golden says otherwise. Goldens and pins regenerate only on the
   Tonality side, and `mts/` is never edited from here.

4. **Port by stability; the Phase 6 fence holds** (Tonality `port/PORT.md`).
   Only the frozen identity substrate is dual-implemented. Phase 6 renegotiates
   "the mask is the key", so porting the analysis, temporal, rules, search or
   pattern layers now would mean porting them twice. Consequence recorded so it
   is not re-litigated: a small ported surface is the INTENDED state of a
   stability-gated port, and currency is measured by the PIN being green rather
   than by tool count.

5. **Parity is byte parity, and byte parity is exact-arithmetic parity.**
   `-ffp-contract=off` repo-wide; `cmul`/`cpowu` encode CPython's compiled-in
   FMA fusion explicitly with `std::fma`; float reductions replicate CPython's
   Neumaier-compensated `sum()`; `pow` is forced through libm via a volatile
   function pointer. Matching libm beats being mathematically nicer — LLVM's
   folded `pow(x,2)→x*x` is 1 ulp *better* than Apple's libm in places, and
   that is a parity failure, not an improvement.

6. **Cross-platform CI split: byte-exact on the generating platform,
   values-within-tolerance everywhere else** (ratified on the engine's dev
   loop, `ratify-ci-required.md`, 2026-07-13; refinements applied in PR #9).
   `macos-15` is canonical and **pinned to the image the fixtures were
   generated on** — measured, not assumed: `macos-14` (also arm64) turned the
   canonical leg red over a single signed zero in `reflection_residual`, so the
   libm VERSION is the binding constraint, not the architecture. `ubuntu-latest`
   stays rolling on purpose (a portability probe wants toolchain drift) and
   compares all 4096 rows within tolerance, with phases compared as complex
   coefficients — a vanishing component's phase is the `atan2` of two
   rounding-noise terms and asserting on it would assert a false invariant.

7. **The RT boundary is between transports, not between fields**
   (2026-08-10, PR #11; answer filed as `response-foundations-rt-questions.md`).
   With a 4096-input domain, every field is available both as live computation
   and as a frozen-table lookup, and those land on opposite sides of the
   real-time line. Two mechanisms landed rather than being described:
   `chirality_slices()` became a `constexpr std::array` (7 allocations and a
   static guard → 0) and `ZTableHandle` turned the Z-table warm-up into a type.
   Recorded because the measurement that motivated it was nearly mis-read: the
   first benchmark pass was ~2× host-throttled and made the constexpr change
   look like a 40% speedup, when an interleaved A/B put it at 1.5%. The change
   is worth keeping for what it removes, not for what it speeds up.
