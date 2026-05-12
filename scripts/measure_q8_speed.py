#!/usr/bin/env python3
"""Measure Q8 AFL++ execution speed variants and preserve raw stats."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise SystemExit(f"{name} not found. Run this inside the fuzz-lab Docker image.")


def run(cmd: list[str], *, env: dict[str, str] | None = None) -> None:
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)


def read_stats(path: Path) -> dict[str, str]:
    stats: dict[str, str] = {}
    for line in path.read_text(errors="replace").splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        stats[key.strip()] = value.strip()
    return stats


def render_report(out_dir: Path, runtime: int) -> str:
    rows = [
        (
            "No sanitizer + fork",
            "`make build-no-asan`",
            "`./png_fuzz_no_asan @@`",
            read_stats(out_dir / "no-asan-fork/default/fuzzer_stats"),
        ),
        (
            "ASan + fork",
            "`make build`",
            "`./png_fuzz @@`",
            read_stats(out_dir / "asan-fork/default/fuzzer_stats"),
        ),
        (
            "ASan + persistent",
            "`make build-persistent`",
            "`./png_fuzz_persistent @@`",
            read_stats(out_dir / "asan-persistent/default/fuzzer_stats"),
        ),
    ]

    lines = [
        "# Q8 - AFL++ Execution-Speed Measurements",
        "",
        f"Method: `afl-fuzz -V {runtime}` fixed-window runs using the same "
        "PNG seed corpus and PNG dictionary.",
        "",
        "| Variant | Build target | Target | Exec/s | Execs done | Target mode | Stability | Edges found | Total edges | Bitmap coverage |",
        "| --- | --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: |",
    ]
    for variant, build, target, stats in rows:
        lines.append(
            f"| {variant} | {build} | {target} | "
            f"{stats.get('execs_per_sec', 'n/a')} | "
            f"{stats.get('execs_done', 'n/a')} | "
            f"{stats.get('target_mode', 'n/a')} | "
            f"{stats.get('stability', 'n/a')} | "
            f"{stats.get('edges_found', 'n/a')} | "
            f"{stats.get('total_edges', 'n/a')} | "
            f"{stats.get('bitmap_cvg', 'n/a')} |"
        )

    lines.extend(
        [
            "",
            "Interpretation: no-sanitizer fork mode removes ASan overhead while "
            "keeping AFL++ compile-time coverage. ASan fork mode pays sanitizer "
            "and process-start costs. ASan persistent mode keeps sanitizer checks "
            "but removes most fork/exec overhead by looping in-process.",
            "",
            "Raw AFL++ stats are saved under:",
            "",
            f"- `{out_dir / 'no-asan-fork/default/fuzzer_stats'}`",
            f"- `{out_dir / 'asan-fork/default/fuzzer_stats'}`",
            f"- `{out_dir / 'asan-persistent/default/fuzzer_stats'}`",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Measure Q8 AFL++ execution speed for fork/persistent variants."
    )
    parser.add_argument("--runtime", type=int, default=int(os.environ.get("Q8_SPEED_RUNTIME", "10")))
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path(os.environ.get("Q8_SPEED_OUT", "evidence/q8-speed")),
    )
    parser.add_argument(
        "--png-dict",
        type=Path,
        default=Path(os.environ.get("PNG_DICT", "/build/dictionaries/png.dict")),
    )
    args = parser.parse_args()

    require_tool("afl-fuzz")
    out_dir = args.out_dir if args.out_dir.is_absolute() else ROOT / args.out_dir
    shutil.rmtree(out_dir / "no-asan-fork", ignore_errors=True)
    shutil.rmtree(out_dir / "asan-fork", ignore_errors=True)
    shutil.rmtree(out_dir / "asan-persistent", ignore_errors=True)
    out_dir.mkdir(parents=True, exist_ok=True)

    run(["make", "build-no-asan"])
    run(["make", "build"])
    run(["make", "build-persistent"])

    base_env = os.environ.copy()
    base_env.update({"AFL_SKIP_CPUFREQ": "1", "AFL_NO_UI": "1"})
    asan_env = base_env.copy()
    asan_env["ASAN_OPTIONS"] = "abort_on_error=1:detect_leaks=0:symbolize=0"

    run(
        [
            "afl-fuzz",
            "-V",
            str(args.runtime),
            "-i",
            "seeds",
            "-o",
            str(out_dir / "no-asan-fork"),
            "-x",
            str(args.png_dict),
            "--",
            "./png_fuzz_no_asan",
            "@@",
        ],
        env=base_env,
    )
    run(
        [
            "afl-fuzz",
            "-V",
            str(args.runtime),
            "-i",
            "seeds",
            "-o",
            str(out_dir / "asan-fork"),
            "-x",
            str(args.png_dict),
            "--",
            "./png_fuzz",
            "@@",
        ],
        env=asan_env,
    )
    run(
        [
            "afl-fuzz",
            "-V",
            str(args.runtime),
            "-i",
            "seeds",
            "-o",
            str(out_dir / "asan-persistent"),
            "-x",
            str(args.png_dict),
            "--",
            "./png_fuzz_persistent",
            "@@",
        ],
        env=asan_env,
    )

    report = render_report(out_dir, args.runtime)
    (out_dir / "README.md").write_text(report)
    print(report, end="")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
