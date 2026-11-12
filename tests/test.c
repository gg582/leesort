/* Correctness test: every type x distribution x length, compared against qsort */
#include "leesort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;
static uint64_t rnd(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

#define CMP(NAME, T) static int cmp_##NAME(const void *a, const void *b) { \
    T x = *(const T *)a, y = *(const T *)b; return (x > y) - (x < y); }

#define CHECK(NAME, T)                                                            \
    CMP(NAME, T)                                                                  \
    static int test_##NAME(void) {                                                \
        static const size_t lens[] = {0,1,2,3,31,32,33,100,1023,1024,1025,5000,100000}; \
        int fails = 0;                                                            \
        for (size_t li = 0; li < sizeof lens / sizeof *lens; li++) {              \
            size_t n = lens[li];                                                  \
            T *a = malloc(sizeof(T) * (n + 1)), *b = malloc(sizeof(T) * (n + 1)); \
            for (int dist = 0; dist < 11; dist++) {                                \
                for (size_t i = 0; i < n; i++) {                                  \
                    uint64_t r = rnd();                                           \
                    switch (dist) {                                               \
                    case 0: a[i] = (T)r; break;                    /* random */   \
                    case 1: a[i] = (T)i; break;                    /* sorted */   \
                    case 2: a[i] = (T)(n - i); break;              /* reverse */  \
                    case 3: a[i] = (T)(r % 4); break;              /* few uniq */ \
                    case 4: a[i] = (T)(r % 16 ? i : r); break;     /* nearly */   \
                    case 5: a[i] = (T)(i % 64); break;             /* sawtooth */ \
                    case 6: a[i] = (T)(i < n / 2 ? i : n - i); break; /* organ */ \
                    case 7: a[i] = (T)(i == 0 ? (T)~(T)0 : (T)i); break; /* one huge value first */ \
                    case 8: a[i] = (T)(i < n - n / 20 ? i : r); break; /* sorted + random tail */ \
                    case 9: a[i] = (T)(i % 8 ? i : n - i); break;  /* every 8th reversed */ \
                    case 10: a[i] = (T)((n - i) / 3); break;       /* reversed w/ dups */ \
                    }                                                             \
                }                                                                 \
                memcpy(b, a, sizeof(T) * n);                                      \
                sort_q_##NAME(a, n);                                              \
                if (n) qsort(b, n, sizeof(T), cmp_##NAME);                        \
                if (n && memcmp(a, b, sizeof(T) * n) != 0) {                      \
                    printf("FAIL %-4s n=%zu dist=%d\n", #NAME, n, dist);          \
                    fails++;                                                      \
                }                                                                 \
            }                                                                     \
            free(a); free(b);                                                     \
        }                                                                         \
        return fails;                                                             \
    }

CHECK(i8, int8_t)   CHECK(u8, uint8_t)   CHECK(i16, int16_t) CHECK(u16, uint16_t)
CHECK(i32, int32_t) CHECK(u32, uint32_t) CHECK(i64, int64_t) CHECK(u64, uint64_t)
CHECK(int, int)     CHECK(uint, unsigned int)

int main(void) {
    int f = test_i8() + test_u8() + test_i16() + test_u16() + test_i32() + test_u32()
          + test_i64() + test_u64() + test_int() + test_uint();
    /* check _Generic dispatch */
    int64_t g[] = {3, -1, 2}; sort_q(g, 3);
    if (!(g[0] == -1 && g[1] == 2 && g[2] == 3)) { puts("FAIL _Generic"); f++; }
    printf(f ? "%d FAILURES\n" : "all tests passed\n", f);
    return f != 0;
}
