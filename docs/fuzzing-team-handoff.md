# Fuzzing Team Handoff

This is the current shared state for the libpng 1.2.56 fuzzing lab work.

## What Is Committed Already

- Reproducible Docker/AFL++ environment for instrumented and QEMU libpng builds.
- PNG seed corpus and AFL++ dictionary wiring.
- Decoder harnesses:
  - `src/harness.c`
  - `src/harness_persistent.c`
- Middle-layer API harnesses:
  - text API / CVE-oriented harness
  - write API harness
  - metadata API harness
- Parallel Makefile targets for local and pwnbox campaigns.
- ASan runtime settings suitable for AFL++ campaigns.
- Plot artifacts for the initial instrumented and QEMU baseline campaigns.

## Newly Shared In This Handoff

- The decoder and persistent harnesses now reject images larger than 4096x4096 before row allocation.
- The decoder and persistent harnesses now exercise background and gamma transform paths before `png_read_update_info`.
- Generated rerun directories are ignored so only deliberate artifacts are committed.

## Campaigns Run So Far

| Campaign | Machine | Runtime | Execs | Coverage / edges | Corpus | Crashes | Hangs | Notes |
| --- | --- | ---: | ---: | --- | ---: | ---: | ---: | --- |
| Decoder, ASan fork | local / pwnbox copy | ~32 min | 1.03M | 30.94%, 966 edges | 715 | 0 | 2 | Baseline instrumented campaign. |
| Decoder, QEMU short | local / pwnbox copy | ~32 min | 1.11M | 4.17%, 2730 edges | 801 | 0 | 0 | Baseline QEMU comparison. |
| Decoder, ASan persistent | pwnbox | ~6 h | 565.6M | 34.75%, 1088 edges | ~1450-1500 per worker | 2 non-repro | 3 | Strong speed/corpus data; crash files did not reproduce standalone with ASan. |
| Text API / CVE | local | ~5 min | ~808k | ~7.5-7.9%, 241-256 edges | ~48-60 per worker | 90 | 0 | Confirmed one real root cause around `png_set_text` / `png_set_text_2`. |
| Write API | pwnbox | ~6 h | 35.7M | 26.70%, 558 edges | 374-397 per worker | 0 | 0 | Negative API campaign. |
| Metadata API clean run | pwnbox | ~6 h | 31.3M | 13.52%, 458 edges | 68-76 per worker | 0 | 0 | Negative API campaign after fixing harness allocation bugs. |
| Decoder, QEMU long | pwnbox | ~3 h 39 min | 45.4M | 4.45%, 2917 edges | ~1333-1378 per worker | 0 | 4 | Stopped/killed before 6 h; still useful for comparison if stated honestly. |

## Findings Status

- Confirmed report-worthy finding: one text API root cause, rediscovering the `png_set_text` / CVE-2016-10087-style path.
- AFL saved 90 crash files for the text API campaign, but they should be treated as duplicates until deduplicated.
- Pending: decoder hangs from fork, persistent, and QEMU campaigns. These need replay, timeout confirmation, and minimization before they are report-worthy.
- Excluded: the first metadata API crash set was caused by our harness fallback allocation bug, then fixed and rerun cleanly.
- Excluded for now: the persistent campaign's two saved crash files did not reproduce as standalone ASan crashes.

## Lab Question Readiness

- Q1 Harness design: ready.
- Q2 Instrumentation/sanitizers/CRC patch: ready.
- Q3 Seeds/dictionary: partial. Need quantitative AFL++ dictionary/havoc/splice yield evidence.
- Q4 Campaign analysis: mostly ready. Need final `afl-plot` outputs and status screenshots for the selected campaigns.
- Q5 Crash triage: almost ready. Need `afl-tmin` on one representative text API crash and a saved ASan trace.
- Q6 Attack surface analysis: needs work. Need two real applications, attack scenarios, and uncovered libpng paths with source references.
- Q7 QEMU comparison: mostly ready. Need side-by-side plot/status artifacts and a careful note that the long QEMU run stopped at ~3 h 39 min.
- Q8 Instrumentation depth/performance: needs work. Need a clean three-way speed experiment and library-vs-harness edge count numbers.

## Actionable Team Tasks

1. Crash triage owner
   - Pick one representative crash from `findings-api-cve-212823`.
   - Run `afl-tmin`.
   - Save the minimized input and final ASan trace.
   - Deduplicate the 90 crash files at the root-cause level.

2. Campaign evidence owner
   - Generate `afl-plot` output for persistent, write API, metadata API, and QEMU long campaigns.
   - Capture AFL++ status screenshots or equivalent evidence for the final report.
   - Extract dictionary/havoc/splice yield evidence for Q3.

3. QEMU comparison owner
   - Produce a same-wall-clock comparison table for instrumented fork vs QEMU.
   - Explain QEMU mode coverage collection and why edge counts/map density differ.
   - Decide whether the 3 h 39 min QEMU long run is acceptable, or rerun it to a clean 6 h timeout.

4. Performance/instrumentation owner
   - Measure exec speed for the same decoder harness under:
     - no sanitizer + fork
     - ASan + fork
     - ASan + persistent
   - Measure/report edge counts for libpng alone versus the final harness binary.
   - Use these numbers for Q8.

5. Attack-surface owner
   - Identify two real applications that use libpng.
   - Write one attack scenario for each.
   - Identify at least two libpng code paths not exercised by our harnesses, with file/function references.

6. Report integration owner
   - Turn the campaign table into the Q4/Q7 narrative.
   - Keep harness bugs separate from libpng findings.
   - State clearly which findings are confirmed, pending, or excluded.
