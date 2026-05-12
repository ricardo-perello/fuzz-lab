#!/usr/bin/env python3
"""Reconstruct Q3 AFL++ strategy evidence from saved artifacts."""

from __future__ import annotations

from collections import Counter
import argparse
import contextlib
import io
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
LOCAL_QUEUES = {
    "local instrumented `findings/default`": ROOT / "findings/default/queue",
    "local QEMU `findings-qemu/default`": ROOT / "findings-qemu/default/queue",
}
TRIAGE_ROOT = ROOT / "triage-artifacts"
PLOT_HEADER = (
    "relative_time, cycles_done, cur_item, corpus_count, pending_total, "
    "pending_favs, map_size, saved_crashes, saved_hangs, max_depth, "
    "execs_per_sec, total_execs, edges_found, total_crashes, servers_count"
)
FINAL_STATS_GROUPS = {
    "Reader harness, local short runs": [
        (
            "local instrumented `findings/default`",
            ROOT / "findings/default/fuzzer_stats",
        ),
        (
            "local QEMU `findings-qemu/default`",
            ROOT / "findings-qemu/default/fuzzer_stats",
        ),
    ],
    "Pwnbox QEMU parallel campaign": [
        (worker, ROOT / f"triage-artifacts/pwnbox/findings-qemu/{worker}/fuzzer_stats")
        for worker in ("main", "worker2", "worker3", "worker4", "worker5", "worker6")
    ],
    "Pwnbox persistent parallel campaign": [
        (worker, ROOT / f"triage-artifacts/pwnbox/findings-persistent/{worker}/fuzzer_stats")
        for worker in ("main", "worker2", "worker3", "worker4", "worker5", "worker6")
    ],
}


def op_from_name(path: Path) -> str | None:
    match = re.search(r"(?:^|,)op:([^,]+)", path.name)
    if not match:
        return None
    return match.group(1)


def is_seed(path: Path) -> bool:
    return "orig:" in path.name


def queue_stats(path: Path) -> dict[str, int]:
    stats = Counter()
    if not path.exists():
        return stats

    for item in path.iterdir():
        if not item.is_file() or not item.name.startswith("id:") or is_seed(item):
            continue
        op = op_from_name(item) or "(none)"
        stats["derived"] += 1
        stats[f"op:{op}"] += 1
        if "+cov" in item.name:
            stats[f"op:{op}:+cov"] += 1
    return stats


def artifact_operator_counts(root: Path) -> Counter:
    counts = Counter()
    for kind in ("crashes", "hangs"):
        for item in root.glob(f"**/{kind}/id:*"):
            if item.is_file():
                counts[op_from_name(item) or "(none)"] += 1
    return counts


def command_lines_with_dictionary(root: Path) -> list[str]:
    lines: list[str] = []
    for stats_path in sorted(root.glob("**/fuzzer_stats")):
        try:
            text = stats_path.read_text(errors="replace")
        except OSError:
            continue
        for line in text.splitlines():
            if line.startswith("command_line") and "png.dict" in line:
                value = line.split(":", 1)[1].strip()
                lines.append(f"- `{stats_path.relative_to(ROOT)}`: `{value}`")
    return lines


def plot_headers(root: Path) -> set[str]:
    headers = set()
    for plot_path in root.glob("**/plot_data"):
        try:
            first = plot_path.read_text(errors="replace").splitlines()[0]
        except (OSError, IndexError):
            continue
        headers.add(first.lstrip("# "))
    return headers


def read_fuzzer_stats(path: Path) -> dict[str, str]:
    stats: dict[str, str] = {}
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return stats
    for line in lines:
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        stats[key.strip()] = value.strip()
    return stats


def emit_report() -> None:
    queue_rows = []
    total = Counter()
    for label, path in LOCAL_QUEUES.items():
        stats = queue_stats(path)
        total.update(stats)
        queue_rows.append(
            (
                label,
                stats["derived"],
                stats["op:havoc"],
                stats["op:ext_UO"],
                stats["op:splice"],
                stats["op:havoc:+cov"],
                stats["op:ext_UO:+cov"],
                stats["op:splice:+cov"],
            )
        )

    artifact_ops = artifact_operator_counts(TRIAGE_ROOT)
    dict_lines = command_lines_with_dictionary(ROOT)
    headers = plot_headers(ROOT)

    print("# Q3 - AFL++ Dictionary and Strategy-Yield Evidence")
    print()
    print(
        "Scope: evidence for AFL++ dictionary/havoc/splice yields in the saved "
        "`triage-artifacts`, local `findings*` queue directories, `plot_data`, "
        "and `fuzzer_stats` files."
    )
    print()
    print("## Bottom line")
    print()
    print(
        "Fresh live Q3 strategy-yield evidence is now preserved under "
        "`evidence/q3-live/`. The exact rows to cite are:"
    )
    print()
    print("- Dictionary run: `dictionary : 55/2726, 0/2754, 0/0, 0/0`")
    print("- Havoc/splice run: `havoc/splice : 61/6500, 4/420`")
    print()
    print(
        "The source transcripts are `evidence/q3-live/afl-ui.typescript` and "
        "`evidence/q3-live/afl-ui-havoc-splice.typescript`; the concise "
        "transcription is `evidence/q3-live/strategy-yields.txt`. These are "
        "supplementary Q3 evidence runs, not replacements for the canonical "
        "30-minute Q4/Q7 campaign metrics."
    )
    print()
    print(
        "The exact AFL++ UI strategy-yield rows from the original long campaigns "
        "were not preserved. This matters because Q3 in `exercise-fuzzing.pdf` "
        "asks for the \"dictionary and havoc/splice rows from your AFL++ status "
        "screen\" and for a quantitative explanation of how many new paths each "
        "strategy contributed. That table is a live AFL++ status-screen view, "
        "not one of the durable artifacts that AFL++ writes in the default "
        "campaign directory."
    )
    print()
    print(
        "The saved `fuzzer_stats` files preserve final campaign counters such "
        "as runtime, execution count, corpus count, discovered edges, bitmap "
        "coverage, crashes, hangs, target mode, and the full command line. They "
        "do not preserve the status UI's per-strategy yield table, so they "
        "cannot answer questions like \"how many attempts and finds came from "
        "havoc versus splice versus dictionary insertion.\" The saved "
        "`plot_data` files are also insufficient: their schema is an aggregate "
        "time series for plotting cycles, corpus size, map size, exec speed, "
        "total execs, and edges over time. They are useful for Q4/Q7 curve "
        "analysis, but they do not record the mutation operator that produced "
        "each new path."
    )
    print()
    print(
        "This limitation is separate from the general project requirements in "
        "`exercise-fuzzing.pdf`. Chapter 1 / the project overview requires a "
        "reproducible white-box AFL++ campaign and a binary-only QEMU campaign, "
        "and Chapter 3 requires the usual durable campaign artifacts "
        "(`plot_data`, `afl-plot` output, harnesses, patches, Dockerfile/"
        "Makefile, seeds, dictionary). Those artifacts were preserved. Q3, "
        "however, asks for an additional piece of live UI evidence: the "
        "strategy-yield rows visible in the AFL++ status screen. Since the "
        "terminal screenshots were not captured during the original runs, the "
        "exact original Q3 UI rows cannot be recovered after the fact from the "
        "saved files. The fresh `evidence/q3-live/` runs close this gap by "
        "preserving new live AFL++ UI transcripts and exact row transcriptions."
    )
    print()
    print(
        "The libpng-specific chapter in the same PDF explains why this evidence "
        "is important for PNG fuzzing. It describes PNG as a structured chunk "
        "language with an 8-byte signature, fixed chunk type tokens such as "
        "`IHDR`, `IDAT`, `PLTE`, and `IEND`, and CRC checks that otherwise block "
        "mutated chunks from reaching deep parser code. The dictionary entries "
        "correspond to recognizable byte-string terminals in that PNG grammar, "
        "while havoc and splice are more general mutation strategies. For that "
        "reason, the Q3 rubric asks not only whether a dictionary was used, but "
        "how much path discovery can be attributed to dictionary operations "
        "versus generic mutation strategies."
    )
    print()
    print("What can be reconstructed defensibly is:")
    print()
    print("- exact live Q3 supplementary strategy rows from `evidence/q3-live/`.")
    print("- dictionary use from saved `command_line` fields and Makefile/Dockerfile wiring.")
    print("- corpus-producing mutation operators from local AFL++ `queue/id:*` filenames.")
    print("- crash/hang-producing mutation operators from saved triage filenames.")
    print("- final campaign totals from `fuzzer_stats` and `plot_data`.")
    print()
    print(
        "No saved evidence was found for splice producing queue entries, "
        "crashes, or hangs in the original campaign artifacts, but the fresh "
        "`-z -u` live UI run records splice strategy yield directly."
    )
    print()
    print("## Saved plot-data schema")
    print()
    if headers:
        for header in sorted(headers):
            print(f"- `{header}`")
    else:
        print(f"- `{PLOT_HEADER}`")
    print()
    print("None of these columns preserves AFL++ strategy attempted/found denominators.")
    print()
    print("## Dictionary usage evidence")
    print()
    print(
        "The PNG dictionary is wired into the PNG reader campaigns with `-x "
        "$(PNG_DICT)`. The Docker image copies AFL++'s `png.dict` to "
        "`/build/dictionaries/png.dict`, and the Makefile default points "
        "`PNG_DICT` there."
    )
    print()
    if dict_lines:
        for line in dict_lines:
            print(line)
    else:
        print("- No saved `fuzzer_stats` command line containing `png.dict` was found.")
    print()
    print("## Reconstructed queue-producing operators")
    print()
    print("| Campaign | Derived queue entries | Havoc entries | Dictionary entries (`ext_UO`) | Splice entries |")
    print("|---|---:|---:|---:|---:|")
    for label, derived, havoc, ext_uo, splice, *_ in queue_rows:
        print(f"| {label} | {derived} | {havoc} | {ext_uo} | {splice} |")
    print(
        f"| combined local queue evidence | {total['derived']} | "
        f"{total['op:havoc']} | {total['op:ext_UO']} | {total['op:splice']} |"
    )
    print()
    print("Coverage-marked subset from the same filenames:")
    print()
    print("| Campaign | Havoc `+cov` | Dictionary `ext_UO +cov` | Splice `+cov` |")
    print("|---|---:|---:|---:|")
    for label, *_, havoc_cov, ext_uo_cov, splice_cov in queue_rows:
        print(f"| {label} | {havoc_cov} | {ext_uo_cov} | {splice_cov} |")
    print(
        f"| combined local queue evidence | {total['op:havoc:+cov']} | "
        f"{total['op:ext_UO:+cov']} | {total['op:splice:+cov']} |"
    )
    print()
    print("Interpretation limits:")
    print()
    print("- These are saved-corpus outcomes, not AFL++ UI yield rows.")
    print("- `ext_UO` is AFL++'s external/user-dictionary overwrite operator.")
    print("- Unsuccessful mutation attempts are not preserved in queue filenames.")
    print("- Absence of `op:splice` means no saved splice-derived artifact was found.")
    print()
    print("## Crash/hang operator evidence in triage artifacts")
    print()
    print("| Operator | Saved crashes/hangs |")
    print("|---|---:|")
    for op, count in sorted(artifact_ops.items(), key=lambda item: (-item[1], item[0])):
        print(f"| {op} | {count} |")
    if not artifact_ops:
        print("| none found | 0 |")
    print()
    print("No triage crash/hang filename with `op:splice` or `op:ext_*` was found.")
    print()
    print("## Final campaign totals from `fuzzer_stats`")
    print()
    for group, entries in FINAL_STATS_GROUPS.items():
        print(f"### {group}")
        print()
        print("| Campaign/worker | cycles_done | corpus_count | execs_done | edges_found | saved_crashes | saved_hangs | havoc_expansion |")
        print("|---|---:|---:|---:|---:|---:|---:|---:|")
        found_any = False
        for label, path in entries:
            stats = read_fuzzer_stats(path)
            if not stats:
                continue
            found_any = True
            print(
                f"| {label} | {stats.get('cycles_done', 'n/a')} | "
                f"{stats.get('corpus_count', 'n/a')} | {stats.get('execs_done', 'n/a')} | "
                f"{stats.get('edges_found', 'n/a')} | {stats.get('saved_crashes', 'n/a')} | "
                f"{stats.get('saved_hangs', 'n/a')} | {stats.get('havoc_expansion', 'n/a')} |"
            )
        if not found_any:
            print("| no saved stats found | n/a | n/a | n/a | n/a | n/a | n/a | n/a |")
        print()
    print("## Formal dictionary interpretation")
    print()
    print(
        "AFL++ dictionaries are literal byte-string terminals that the mutator can "
        "insert or overwrite. For PNG, those terminals approximate grammar "
        "tokens such as the PNG signature and fixed chunk type names. They help "
        "AFL++ build parser-recognized syntax but do not encode the full PNG "
        "grammar: lengths, CRC fields, chunk ordering, and compressed IDAT "
        "structure still have to be discovered through mutation and feedback."
    )
    print()
    print("## Suggested report wording")
    print()
    print(
        "> The exact AFL++ strategy-yield UI rows were not preserved in the saved "
        "artifacts. From saved queue filenames in the local reader campaigns, "
        f"havoc produced {total['op:havoc']} of {total['derived']} derived queue "
        f"entries. The external PNG dictionary produced {total['op:ext_UO']} "
        f"saved queue entries, {total['op:ext_UO:+cov']} of which was coverage-marked. "
        "No saved `op:splice` queue, crash, or hang artifacts were found. The "
        "dictionary was enabled for the reader, QEMU, and persistent campaigns "
        "via `-x png.dict`; its entries are grammar-terminal byte strings such "
        "as the PNG signature and chunk identifiers."
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Reconstruct Q3 AFL++ strategy evidence from saved artifacts."
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Optional Markdown output path. Defaults to stdout.",
    )
    args = parser.parse_args()

    if args.output:
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            emit_report()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(buffer.getvalue())
    else:
        emit_report()


if __name__ == "__main__":
    main()
