# leesort

Adaptive integer sort for C. A single scan classifies the input, then dispatches to the
cheapest strategy:

| Input shape (detected in one pass)        | Strategy                                                        |
|-------------------------------------------|-----------------------------------------------------------------|
| already sorted / non-increasing           | return / reverse, O(n)                                          |
| nearly sorted (few descents)              | **cherry-pick**: extract the outliers, sort them, merge back    |
| few monotone runs (organ pipe, blocks)    | natural merge over run boundaries, O(n log runs)                |
| everything else                           | LSD radix on `x - min`, one pass per byte that actually varies  |

Supported types: `int8/16/32/64_t`, `uint8/16/32/64_t`, `int`, `unsigned int`.

## Build

```sh
make            # build/libleesort.a, build/libleesort.so, test and bench binaries
make test       # correctness vs. qsort (all types, many distributions and lengths)
make bench      # leesort vs. libc qsort (also available as leesort_bench() in the library)
make install PREFIX=/usr/local
```

Default flags: `-O3 -march=native -fomit-frame-pointer`.

```c
#include <leesort.h>

int32_t a[] = {3, -1, 2};
sort_q_i32(a, 3);   /* or sort_q(a, 3) with C11 _Generic */
```

Link with `-lleesort`.

## Comparison with other sorts

`make compare` benchmarks leesort against pdqsort, ska_sort, x86-simd-sort (AVX2),
`std::sort`, `std::stable_sort`, TimSort, a plain LSD radix and qsort.
Fetch the pinned third-party sources first with `bench/fetch_baselines.sh`.

Results on an AMD Ryzen 5 5600X (AVX2, no AVX-512), GCC 14, pinned to one core,
best of 3+ runs, every output verified against `std::sort`. Raw data:
[`bench/results/compare-avx2.csv`](bench/results/compare-avx2.csv).

N = 10,000,000, milliseconds (lower is better):

| distribution  | leesort | radix | ska_sort | x86simd | pdqsort | std::sort | timsort | qsort |
|---------------|--------:|------:|---------:|--------:|--------:|----------:|--------:|------:|
| **i32** random        |  93.0 |  88.4 | 149.4 | **51.5** | 211.5 | 518.2 | 764.6 | 1057.6 |
| i32 sorted            |   3.1 |  74.1 |  56.3 |  45.8 |   4.5 |  86.1 | **2.3** | 270.0 |
| i32 reversed          | **5.5** | 74.3 |  57.4 |  46.2 |  10.8 |  56.4 |  13.6 | 316.4 |
| i32 nearly 0.1%       | **23.4** | 75.3 | 76.0 |  49.4 |  50.4 |  87.8 |  98.1 | 415.3 |
| i32 nearly 1%         | **27.7** | 80.9 | 94.2 |  53.9 |  77.9 |  94.3 | 136.9 | 454.0 |
| i32 nearly 10%        |  75.9 |  77.5 | 104.5 | **63.4** | 137.1 | 181.1 | 276.5 | 588.1 |
| i32 few unique        |  33.5 |  35.7 |  26.6 | **8.6** |  30.6 | 182.1 | 356.6 | 639.4 |
| i32 sawtooth          |  46.4 |  45.4 |  30.9 | **28.6** |  79.6 | 177.6 | 110.4 | 426.5 |
| i32 organ pipe        |  31.8 |  72.1 |  66.0 |  55.1 | 227.0 | 484.9 | **16.3** | 308.9 |
| i32 sorted + 5% tail  | **33.5** | 74.6 | 128.5 | 51.1 | 181.5 | 485.4 |  44.0 | 309.7 |
| i32 jitter            |  97.5 |  95.1 | 138.7 | **45.9** | 142.3 | 153.8 | 162.4 | 380.4 |
| i32 64 sorted blocks  |  96.9 |  86.1 | 174.8 | **63.0** | 222.8 | 287.8 | 194.5 | 484.2 |
| i32 mixed blocks      |  97.1 |  93.2 | 184.3 | **57.7** | 213.6 | 425.1 | 384.6 | 708.8 |
| **u64** random        | 198.4 | 175.9 | 156.1 | **107.9** | 214.2 | 525.9 | 779.9 | 1060.1 |
| u64 sorted            |   3.5 | 105.6 |  71.8 | 107.1 |   5.3 |  86.6 | **3.0** | 253.9 |
| u64 reversed          | **7.6** | 105.2 | 73.6 | 106.8 |  12.7 |  60.5 |  14.9 | 373.8 |
| u64 nearly 0.1%       | **24.6** | 103.9 | 91.8 | 107.2 |  50.7 |  89.1 | 103.2 | 415.2 |
| u64 nearly 1%         | **29.0** | 102.2 | 109.7 | 109.1 |  79.0 |  96.8 | 146.5 | 466.0 |
| u64 nearly 10%        | **83.2** | 99.8 | 122.4 | 118.4 | 139.3 | 187.5 | 286.4 | 601.0 |
| u64 few unique        |  39.7 |  64.3 |  42.2 | **17.9** |  32.5 | 168.0 | 365.2 | 660.1 |
| u64 sawtooth          |  55.7 |  91.4 | **52.9** | 62.9 |  84.4 | 175.0 | 110.3 | 433.1 |
| u64 organ pipe        |  31.4 | 107.4 |  80.9 | 120.0 | 227.5 | 501.1 | **17.5** | 327.6 |
| u64 sorted + 5% tail  | **36.1** | 122.6 | 145.9 | 110.1 | 183.5 | 538.5 |  44.0 | 292.9 |
| u64 jitter            | **105.0** | 111.0 | 154.7 | 108.0 | 144.7 | 158.3 | 180.5 | 364.4 |
| u64 64 sorted blocks  | **106.3** | 115.9 | 188.8 | 131.0 | 221.1 | 285.0 | 201.4 | 474.4 |
| u64 mixed blocks      | **107.5** | 118.8 | 200.8 | 115.4 | 214.4 | 425.7 | 405.9 | 718.9 |

### Where leesort stands

- **Wins:** nearly sorted inputs (2x faster than the best alternative at 0.1-1% disorder),
  reversed inputs, sorted data with an appended tail, and most structured `u64` inputs.
- **Loses to x86-simd-sort** on random and random-like `i32` data (about 1.8x) and on few-unique
  data (2-4x). On AVX-512 hardware the gap would be larger.
- **Loses to TimSort** on organ pipe (about 1.9x) and slightly on already-sorted input (the
  classification scan costs more than TimSort's run check).
- Against a plain LSD radix on random data the classification overhead is about 5% (`i32`)
  to 13% (`u64`).

Other limitations: O(n) extra memory for the radix and merge paths (an in-place heapsort is
used if allocation fails), integers only, no custom comparators.
