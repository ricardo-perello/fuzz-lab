# Q3 - AFL++ Strategy-Yield Evidence

Method: two short AFL++ UI capture runs using the decoder harness, the PNG seed
corpus, and `/build/dictionaries/png.dict`.

The exact AFL++ strategy-yield rows are transcribed in
`evidence/q3-live/strategy-yields.txt` and backed by the terminal transcripts
`evidence/q3-live/afl-ui.typescript` and
`evidence/q3-live/afl-ui-havoc-splice.typescript`.

| Run | Strategy row | Finds | Attempts | Yield |
| --- | --- | ---: | ---: | ---: |
| Dictionary capture | Dictionary overwrite | 55 | 2726 | 2.02% |
| Dictionary capture | Dictionary insert | 0 | 2754 | 0.00% |
| Havoc/splice capture | Havoc | 61 | 6500 | 0.94% |
| Havoc/splice capture | Splice | 4 | 420 | 0.95% |

Interpretation: the PNG dictionary supplies fixed grammar-like byte tokens such
as the PNG signature and chunk names (`IHDR`, `PLTE`, `IDAT`, `IEND`). Dictionary
overwrite was the highest-yield operator in this sample, while havoc and splice
still contributed new paths through broader structural mutations.

Raw AFL++ stats for the two runs are saved under:

- `evidence/q3-live/findings/default/fuzzer_stats`
- `evidence/q3-live/findings-havoc-splice/default/fuzzer_stats`
