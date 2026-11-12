#define _POSIX_C_SOURCE 199309L
#include "leesort.h"
#include "leesort_bench.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t lb_rng;
static uint64_t lb_rnd(void) {
    uint64_t z = (lb_rng += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static double lb_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static const char *lb_dist_names[] = {
    "random", "sorted", "reversed", "nearly_sorted", "few_unique", "sawtooth", "organ_pipe"
};
enum { LB_NDIST = sizeof lb_dist_names / sizeof *lb_dist_names };

/* Scale i to the type's range so sorted/reversed inputs are actually monotonic */
#define LB_TYPE(NAME, T, TMAX)                                                         \
static int lb_cmp_##NAME(const void *a, const void *b) {                               \
    T x = *(const T *)a, y = *(const T *)b;                                            \
    return (x > y) - (x < y);                                                          \
}                                                                                      \
static void lb_fill_##NAME(T *a, size_t n, int dist) {                                 \
    lb_rng = 12345 + (uint64_t)dist;                                                   \
    double scale = n > (size_t)(TMAX) ? (double)(TMAX) / (double)n : 1.0;              \
    for (size_t i = 0; i < n; i++) {                                                   \
        uint64_t r = lb_rnd();                                                         \
        T lin = (T)((double)i * scale), rev = (T)((double)(n - i) * scale);            \
        switch (dist) {                                                                \
        case 0: a[i] = (T)r; break;                                                    \
        case 1: case 3: a[i] = lin; break;                                             \
        case 2: a[i] = rev; break;                                                     \
        case 4: a[i] = (T)(r % 16); break;                                             \
        case 5: a[i] = (T)(i % 1000); break;                                           \
        case 6: a[i] = i < n / 2 ? lin : rev; break;                                   \
        }                                                                              \
    }                                                                                  \
    if (dist == 3) /* swap 1% of elements with random positions */                                    \
        for (size_t k = 0; k < n / 100; k++) {                                         \
            size_t x = lb_rnd() % n, y = lb_rnd() % n;                                 \
            T t = a[x]; a[x] = a[y]; a[y] = t;                                         \
        }                                                                              \
}                                                                                      \
static int lb_run_##NAME(FILE *out, size_t n) {                                        \
    T *src = malloc(sizeof(T) * n), *w = malloc(sizeof(T) * n),                        \
      *ref = malloc(sizeof(T) * n);                                                    \
    int fails = 0;                                                                     \
    if (!src || !w || !ref) { free(src); free(w); free(ref); return 1; }               \
    int reps = (int)(20000000 / (n + 1));                                              \
    if (reps < 3) reps = 3;                                                            \
    if (reps > 2000) reps = 2000;                                                      \
    for (int d = 0; d < LB_NDIST; d++) {                                               \
        lb_fill_##NAME(src, n, d);                                                     \
        double best_l = 1e30, best_q = 1e30;                                           \
        for (int r = 0; r < reps; r++) {                                               \
            memcpy(w, src, sizeof(T) * n);                                             \
            double t0 = lb_now(); sort_q_##NAME(w, n); double t1 = lb_now();           \
            if (t1 - t0 < best_l) best_l = t1 - t0;                                    \
            memcpy(ref, src, sizeof(T) * n);                                           \
            t0 = lb_now(); qsort(ref, n, sizeof(T), lb_cmp_##NAME); t1 = lb_now();     \
            if (t1 - t0 < best_q) best_q = t1 - t0;                                    \
        }                                                                              \
        int ok = memcmp(w, ref, sizeof(T) * n) == 0;                                   \
        fails += !ok;                                                                  \
        fprintf(out, "%-4s %10zu  %-14s %12.3f %12.3f %8.2fx%s\n", #NAME, n,           \
                lb_dist_names[d], best_l * 1e3, best_q * 1e3, best_q / best_l,         \
                ok ? "" : "  MISMATCH");                                               \
    }                                                                                  \
    free(src); free(w); free(ref);                                                     \
    return fails;                                                                      \
}

LB_TYPE(i16, int16_t, INT16_MAX)
LB_TYPE(i32, int32_t, INT32_MAX)
LB_TYPE(u64, uint64_t, UINT32_MAX)

int leesort_bench(FILE *out, const size_t *sizes, size_t nsizes, unsigned type_mask) {
    static const size_t def[] = {1000, 100000, 1000000, 10000000};
    if (!sizes || nsizes == 0) { sizes = def; nsizes = sizeof def / sizeof *def; }
    if (!type_mask) type_mask = LEESORT_BENCH_ALL;
    int fails = 0;
    fprintf(out, "%-4s %10s  %-14s %12s %12s %9s\n",
            "type", "N", "dist", "leesort(ms)", "qsort(ms)", "speedup");
    if (type_mask & LEESORT_BENCH_I32) for (size_t i = 0; i < nsizes; i++) fails += lb_run_i32(out, sizes[i]);
    if (type_mask & LEESORT_BENCH_U64) for (size_t i = 0; i < nsizes; i++) fails += lb_run_u64(out, sizes[i]);
    if (type_mask & LEESORT_BENCH_I16) for (size_t i = 0; i < nsizes; i++) fails += lb_run_i16(out, sizes[i]);
    return fails;
}
