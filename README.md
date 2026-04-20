# CS-412 Fuzzing Lab — libpng 1.2.56

AFL++ coverage-guided fuzzing campaign against **libpng 1.2.56**.

## Quick Start

```bash
docker build -t fuzz-lab .
docker run --rm -it --privileged \
    -v "$PWD/src:/build/src" \
    -v "$PWD/seeds:/build/seeds" \
    -v "$PWD/findings:/build/findings" \
    fuzz-lab
# inside container:
make fuzz
```

`--privileged` is required for AFL++ to set `/proc/sys/kernel/core_pattern`.  
`src/` and `seeds/` are volume-mounted so Person B can edit harness code without rebuilding the image.

## Targets

| Target | Description |
|---|---|
| `make build` | Compile instrumented harness (`png_fuzz`) |
| `make build-qemu` | Compile vanilla harness (`png_fuzz_qemu`) |
| `make fuzz` | Run instrumented AFL++ campaign |
| `make fuzz-qemu` | Run QEMU-mode AFL++ campaign |
| `make clean` | Remove binaries and findings |

## Generate plots

```bash
afl-plot findings/default plot_output
afl-plot findings-qemu/default plot_output_qemu
```

## Repository layout

```
Dockerfile          — AFL++ toolchain + both libpng builds (baked in)
Makefile
src/                — harness.c, harness_persistent.c (Person B)
patches/            — CRC-neutralization patch for libpng 1.2.x
seeds/              — seed PNG corpus (Person B)
dictionaries/       — png.dict (copied from AFL++ at build time)
findings/           — instrumented campaign output (gitignored)
findings-qemu/      — QEMU campaign output (gitignored)
```
