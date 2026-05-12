#!/usr/bin/env python3
"""Measure library-alone versus final-harness AFL++ edge counts."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
PROBE_SOURCE = """#include <png.h>

int main(void) {
    png_structp png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING,
        0,
        0,
        0
    );
    if (!png) {
        return 1;
    }
    png_destroy_read_struct(&png, 0, 0);
    return 0;
}
"""


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
            "Minimal libpng probe",
            "`afl-clang-fast -g -O1` + `/build/install_no_asan/lib/libpng12.a`",
            read_stats(out_dir / "libpng-probe/default/fuzzer_stats"),
        ),
        (
            "Final decoder harness",
            "`make build-no-asan`",
            read_stats(out_dir / "final-harness/default/fuzzer_stats"),
        ),
    ]

    lines = [
        "# Q8 - Library-Alone vs Final-Harness Edge Counts",
        "",
        f"Method: `afl-fuzz -V {runtime}` short runs using the ASan-free "
        "AFL-instrumented libpng build.",
        "",
        "The library-alone row is a minimal linked probe that calls "
        "`png_create_read_struct()` and `png_destroy_read_struct()`. It is a "
        "proxy for the linked instrumented libpng surface, not a full parser "
        "campaign.",
        "",
        "| Binary | Runtime setup | Edges found | Total edges | Bitmap coverage | Exec/s |",
        "| --- | --- | ---: | ---: | ---: | ---: |",
    ]

    for name, setup, stats in rows:
        lines.append(
            f"| {name} | {setup} | {stats.get('edges_found', 'n/a')} | "
            f"{stats.get('total_edges', 'n/a')} | {stats.get('bitmap_cvg', 'n/a')} | "
            f"{stats.get('execs_per_sec', 'n/a')} |"
        )

    lines.extend(
        [
            "",
            "Interpretation: compare `total_edges` to estimate the instrumented "
            "edge universe linked into each binary, and compare `edges_found` to "
            "show how much additional code is actually reached when the full "
            "harness feeds PNG bytes through `png_read_info()`, transform setup, "
            "row allocation, `png_read_image()`, and `png_read_end()`.",
            "",
            "Raw AFL++ stats are saved under:",
            "",
            f"- `{out_dir / 'libpng-probe/default/fuzzer_stats'}`",
            f"- `{out_dir / 'final-harness/default/fuzzer_stats'}`",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Measure Q8 AFL++ edge counts for libpng probe and decoder harness."
    )
    parser.add_argument("--runtime", type=int, default=int(os.environ.get("Q8_EDGE_RUNTIME", "5")))
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path(os.environ.get("Q8_EDGE_OUT", "evidence/q8-edge-counts")),
    )
    parser.add_argument(
        "--libpng-prefix",
        type=Path,
        default=Path(os.environ.get("INSTALL_DIR_NO_ASAN", "/build/install_no_asan")),
    )
    parser.add_argument(
        "--png-dict",
        type=Path,
        default=Path(os.environ.get("PNG_DICT", "/build/dictionaries/png.dict")),
    )
    args = parser.parse_args()

    require_tool("afl-clang-fast")
    require_tool("afl-fuzz")

    libpng_archive = args.libpng_prefix / "lib/libpng12.a"
    if not libpng_archive.is_file():
        raise SystemExit(f"Missing {libpng_archive}. Build the Docker image first.")

    out_dir = args.out_dir
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir
    seed_dir = out_dir / "seed-libpng-probe"
    shutil.rmtree(out_dir / "libpng-probe", ignore_errors=True)
    shutil.rmtree(out_dir / "final-harness", ignore_errors=True)
    seed_dir.mkdir(parents=True, exist_ok=True)
    (seed_dir / "seed").write_text("probe\n")

    probe_c = out_dir / "libpng_probe.c"
    probe_bin = out_dir / "png_libpng_probe"
    probe_c.write_text(PROBE_SOURCE)

    run(
        [
            "afl-clang-fast",
            "-g",
            "-O1",
            str(probe_c),
            f"-I{args.libpng_prefix / 'include'}",
            str(libpng_archive),
            "-lz",
            "-lm",
            "-o",
            str(probe_bin),
        ]
    )
    run(["make", "build-no-asan"])

    env = os.environ.copy()
    env.update(
        {
            "AFL_SKIP_CPUFREQ": "1",
            "AFL_NO_UI": "1",
            "ASAN_OPTIONS": "abort_on_error=1:detect_leaks=0:symbolize=0",
        }
    )

    run(
        [
            "afl-fuzz",
            "-V",
            str(args.runtime),
            "-i",
            str(seed_dir),
            "-o",
            str(out_dir / "libpng-probe"),
            "--",
            str(probe_bin),
            "@@",
        ],
        env=env,
    )
    run(
        [
            "afl-fuzz",
            "-V",
            str(args.runtime),
            "-i",
            "seeds",
            "-o",
            str(out_dir / "final-harness"),
            "-x",
            str(args.png_dict),
            "--",
            "./png_fuzz_no_asan",
            "@@",
        ],
        env=env,
    )

    report = render_report(out_dir, args.runtime)
    (out_dir / "README.md").write_text(report)
    print(report, end="")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
