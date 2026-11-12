#ifndef LEESORT_BENCH_H
#define LEESORT_BENCH_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LEESORT_BENCH_I16 = 1u << 0,
    LEESORT_BENCH_I32 = 1u << 1,
    LEESORT_BENCH_U64 = 1u << 2,
    LEESORT_BENCH_ALL = 0x7u
};

/* Benchmark leesort against libc qsort over several distributions and sizes,
 * printing a table to `out`. Every result is checked against qsort's output;
 * returns the number of mismatches (0 on success). */
int leesort_bench(FILE *out, const size_t *sizes, size_t nsizes, unsigned type_mask);

#ifdef __cplusplus
}
#endif

#endif /* LEESORT_BENCH_H */
