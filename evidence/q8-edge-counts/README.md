# Q8 - Library-Alone vs Final-Harness Edge Counts

Method: `afl-fuzz -V 5` short runs using the ASan-free AFL-instrumented libpng build.

The library-alone row is a minimal linked probe that calls `png_create_read_struct()` and `png_destroy_read_struct()`. It is a proxy for the linked instrumented libpng surface, not a full parser campaign.

| Binary | Runtime setup | Edges found | Total edges | Bitmap coverage | Exec/s |
| --- | --- | ---: | ---: | ---: | ---: |
| Minimal libpng probe | `afl-clang-fast -g -O1` + `/build/install_no_asan/lib/libpng12.a` | 49 | 3090 | 1.59% | 193.00 |
| Final decoder harness | `make build-no-asan` | 638 | 3130 | 20.38% | 100.48 |

Interpretation: compare `total_edges` to estimate the instrumented edge universe linked into each binary, and compare `edges_found` to show how much additional code is actually reached when the full harness feeds PNG bytes through `png_read_info()`, transform setup, row allocation, `png_read_image()`, and `png_read_end()`.

Raw AFL++ stats are saved under:

- `/work/evidence/q8-edge-counts/libpng-probe/default/fuzzer_stats`
- `/work/evidence/q8-edge-counts/final-harness/default/fuzzer_stats`
