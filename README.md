# CS-412 Fuzzing Lab — libpng 1.2.56

AFL++ coverage-guided fuzzing campaign against **libpng 1.2.56**.

---

## What's already done

The Docker image builds cleanly end-to-end. Inside the container you have:

- `afl-fuzz`, `afl-clang-fast`, `afl-qemu-trace`, `afl-tmin`, `afl-plot` on PATH
- `/build/install/lib/libpng12.a` — libpng compiled with `afl-clang-fast` + ASan (instrumented)
- `/build/install_vanilla/lib/libpng12.a` — libpng compiled with plain `gcc`, no sanitizers (for QEMU mode)
- `/build/dictionaries/png.dict` — AFL++'s built-in PNG token dictionary, originally attributed to Michal Zalewski
- `/build/Makefile` — all build and fuzz targets wired up

The completed repository includes the harnesses, seed corpus, campaign evidence,
triage artifacts, and report source needed for the final submission.

---

## Prerequisites

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) installed and running
- Clone this repo

---

## Docker Image Setup

```bash
docker build -t fuzz-lab .
```

This takes ~10 minutes once and is cached after that. You only need to re-run it if the Dockerfile changes.

---

## Harnesses and Seeds

The repository includes the main decoder harness, a persistent-mode variant,
focused API harnesses, and a small PNG seed corpus.

### `src/harness.c`

This is the entry point AFL++ drives. It:
1. Reads a file from `argv[1]` (AFL++ passes `@@`, which becomes the mutated input path)
2. Feeds it to libpng via the `png_set_read_fn()` callback API
3. Calls `png_read_info()` and `png_read_image()` to trigger the full parse pipeline
4. Cleans up and exits without interactive prompts

The typical libpng decode sequence (from the exercise guide):

```c
png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
png_infop   info = png_create_info_struct(png);

// set up your read callback instead of a FILE*
png_set_read_fn(png, &my_state, my_read_callback);

// parse header + all pre-IDAT chunks
png_read_info(png, info);

// optionally apply transforms so all color types are exercised uniformly
png_set_expand(png);
png_set_strip_16(png);
png_set_gray_to_rgb(png);
png_read_update_info(png, info);

// decompress + defilter pixel data — this is where most CVEs live
png_read_image(png, row_pointers);

png_read_end(png, NULL);
png_destroy_read_struct(&png, &info, NULL);
```

**Error handling:** libpng uses `setjmp`/`longjmp` for errors. The harness sets up a `setjmp` point after `png_create_read_struct` so malformed inputs return cleanly instead of aborting the fuzzer process.

```c
if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, NULL);
    // free anything else, then return cleanly
    return 1;
}
```

**Include path inside the container:** `-I/build/install/include`

### `src/harness_persistent.c`

Same logic as `harness.c` but wrapped in AFL++'s persistent loop macro to avoid fork overhead (needed for Q8):

```c
while (__AFL_LOOP(1000)) {
    // re-read input and run the full parse pipeline
    // reset all state between iterations
}
```

### `seeds/`

The seed corpus contains ten small valid PNGs covering RGB, RGBA, grayscale,
grayscale+alpha, indexed color, text metadata, Adam7 interlacing, and
multi-IDAT cases. The seeds start with the 8-byte PNG magic
`\x89PNG\r\n\x1a\n`, which gets AFL++ past libpng's early signature gate.

---

## Docker Workflow

Every session:

```bash
# start the container with your local src/ and seeds/ visible inside it
docker run --rm -it --privileged \
    -v "$PWD/src:/build/src" \
    -v "$PWD/seeds:/build/seeds" \
    -v "$PWD/findings:/build/findings" \
    -v "$PWD/findings-qemu:/build/findings-qemu" \
    fuzz-lab
```

The `-v` flags are volume mounts — your local files appear at `/build/src` etc. inside the container. **Edit files on your machine with your normal editor; the container sees changes immediately.**

Inside the container:

```bash
make build        # compile src/harness.c → ./png_fuzz  (instrumented)
make fuzz         # start the instrumented campaign (runs until Ctrl+C)

make build-qemu   # compile src/harness.c → ./png_fuzz_qemu  (vanilla/QEMU)
make fuzz-qemu    # start the QEMU-mode campaign
```

If `make build` fails, fix `src/harness.c` on your host machine and re-run `make build` — no need to restart the container.

---

## Campaigns and Evidence

### Running campaigns

Each campaign must run for **at least 30 minutes**. Run them in separate terminal sessions (or sequentially):

```bash
# session 1: instrumented
make fuzz          # outputs to findings/default/

# session 2: QEMU
make fuzz-qemu     # outputs to findings-qemu/default/
```

While running, AFL++ shows a status screen. Key metrics for the report:
- `stability` — how reproducibly the same input produces the same coverage (should be >95%)
- `map density` — percentage of the coverage bitmap that has been hit
- `corpus count` — number of unique inputs saved
- `cycles done` — how many times the fuzzer has cycled through the full corpus
- `exec speed` — executions per second

### Generating plots

After each campaign (inside the container):

```bash
afl-plot findings/default plot_output
afl-plot findings-qemu/default plot_output_qemu
```

This generates `plot_output/index.html` with edges-over-time graphs. The report appendix uses the generated plot artifacts plus the matching `fuzzer_stats` files as AFL++ status evidence.

### Crash triage evidence (Q5)

The reportable crash comes from the focused text API campaign, not the baseline
decoder campaign. The original AFL++ crash corpus is under
`triage-artifacts/local/findings-api-cve-212823/`, and the final reproducer,
minimized input, ASan trace, `afl-tmin` output, and deduplication logs are under
`pocs/text_api_triage/`.

The triage flow used for the representative crash was:

```bash
# minimize the crashing input to the smallest reproducer with the text API harness
afl-tmin -i <crash-file> -o minimized_input -- ./png_fuzz_api @@

# get the ASan stack trace
./png_fuzz_api minimized_input
```

### Measuring exec speed for Q8

The checked-in Q8 evidence was generated with the helper scripts:

```bash
python3 scripts/measure_q8_edges.py --runtime 30
python3 scripts/measure_q8_speed.py --runtime 30
```

The resulting `fuzzer_stats`, `plot_data`, and summary tables are stored under
`evidence/q8-edge-counts/` and `evidence/q8-speed/`.

---

## Repository layout

```
Dockerfile              — builds the entire environment (do not edit unless dependencies change)
Makefile                — all build and fuzz targets
src/
  harness.c             — main decoder fuzzing harness
  harness_persistent.c  — persistent-mode decoder variant for Q8
patches/
  libpng-1.2.56-no-crc.patch  — neutralizes CRC checks so mutations reach deep parser code
seeds/                  — seed PNG corpus
dictionaries/           — png.dict, AFL++ built-in PNG dictionary originally attributed to Michal Zalewski
findings/               — source-instrumented campaign evidence
findings-qemu/          — QEMU campaign evidence
plot_output/            — afl-plot output for instrumented campaign
plot_output_qemu/       — afl-plot output for QEMU campaign
pocs/text_api_triage/   — minimized text API crash reproducer and ASan evidence
evidence/               — Q3 and Q8 supporting measurement artifacts
```

---

## Why two builds of libpng?

The lab has two modes that answer different research questions:

| | Instrumented | QEMU |
|---|---|---|
| Compiler | `afl-clang-fast` | `gcc` |
| Sanitizers | ASan | none |
| Coverage | compile-time edge counters | QEMU translates binary at runtime and injects counters |
| Exec speed | fast | ~2–5× slower |
| Use case | white-box fuzzing (source available) | black-box fuzzing (binary only) |

The QEMU run simulates what you'd do against a closed-source binary where you can't recompile with instrumentation.

## Why is the CRC patch critical?

Every PNG chunk ends with a CRC-32 checksum. When AFL++ mutates a chunk's data, the checksum no longer matches. libpng's default behavior:
- **Critical chunks** (IHDR, PLTE, IDAT, IEND): call `png_error()` → `longjmp` → abort parsing immediately
- **Ancillary chunks**: silently discard the chunk

In both cases, AFL++'s mutations never reach the deep parsing code where bugs actually live. The patch makes `png_crc_finish()` always return success, so all mutations propagate into the parser.
