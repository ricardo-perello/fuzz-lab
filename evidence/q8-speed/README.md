# Q8 - AFL++ Execution-Speed Measurements

Method: `afl-fuzz -V 10` fixed-window runs using the same PNG seed corpus and PNG dictionary.

| Variant | Build target | Target | Exec/s | Execs done | Target mode | Stability | Edges found | Total edges | Bitmap coverage |
| --- | --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: |
| No sanitizer + fork | `make build-no-asan` | `./png_fuzz_no_asan @@` | 73.61 | 738 | shmem_testcase default | 100.00% | 645 | 3130 | 20.61% |
| ASan + fork | `make build` | `./png_fuzz @@` | 62.80 | 630 | shmem_testcase default | 100.00% | 642 | 3129 | 20.52% |
| ASan + persistent | `make build-persistent` | `./png_fuzz_persistent @@` | 81.99 | 823 | persistent shmem_testcase | 100.00% | 656 | 3138 | 20.91% |

Interpretation: no-sanitizer fork mode removes ASan overhead while keeping AFL++ compile-time coverage. ASan fork mode pays sanitizer and process-start costs. ASan persistent mode keeps sanitizer checks but removes most fork/exec overhead by looping in-process.

Raw AFL++ stats are saved under:

- `/work/evidence/q8-speed/no-asan-fork/default/fuzzer_stats`
- `/work/evidence/q8-speed/asan-fork/default/fuzzer_stats`
- `/work/evidence/q8-speed/asan-persistent/default/fuzzer_stats`
