# Q8 - AFL++ Execution-Speed Measurements

Method: `afl-fuzz -V 30` fixed-window runs using the same PNG seed corpus and PNG dictionary.

| Variant | Build target | Target | Exec/s | Execs done | Target mode | Stability | Edges found | Total edges | Bitmap coverage |
| --- | --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: |
| No sanitizer + fork | `make build-no-asan` | `./png_fuzz_no_asan @@` | 1612.57 | 48403 | shmem_testcase default | 100.00% | 793 | 3130 | 25.34% |
| ASan + fork | `make build` | `./png_fuzz @@` | 684.78 | 20549 | shmem_testcase default | 100.00% | 752 | 3129 | 24.03% |
| ASan + persistent | `make build-persistent` | `./png_fuzz_persistent @@` | 1799.32 | 53985 | persistent shmem_testcase | 99.87% | 788 | 3138 | 25.11% |

Interpretation: no-sanitizer fork mode removes ASan overhead while keeping AFL++ compile-time coverage. ASan fork mode pays sanitizer and process-start costs. ASan persistent mode keeps sanitizer checks but removes most fork/exec overhead by looping in-process.

Raw AFL++ stats are saved under:

- `/work/evidence/q8-speed/no-asan-fork/default/fuzzer_stats`
- `/work/evidence/q8-speed/asan-fork/default/fuzzer_stats`
- `/work/evidence/q8-speed/asan-persistent/default/fuzzer_stats`
