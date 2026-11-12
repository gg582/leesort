#include "leesort.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define SORT_Q_SMALL_THRESHOLD 32    /* below this length: insertion sort        */
#define SORT_Q_MINRUN          32    /* natural merge: extend short runs to this */
#define SORT_Q_RADIX_THRESHOLD 128   /* radix considered only above this length  */
#define SORT_Q_PICK_DIV        16    /* try cherry-pick if descents <= len/16    */
#define SORT_Q_PICK_LIMIT_DIV  8     /* give up if more than len/8 are picked    */
#define SORT_Q_PICK_ASC_RATIO  8     /* ...and only if ascents >= 8 * descents   */
#define SORT_Q_MERGE_PASS_COST 2     /* measured: 1 merge pass ~= 2 radix passes */

static unsigned sort_q_ceil_log2(size_t x) {
    unsigned r = 0;
    while (((size_t)1 << r) < x) r++;
    return r;
}

#define SORT_Q_DEFINE(NAME, TYPE, UTYPE, IS_SIGNED)                                            \
/* arr[0..sorted) is already sorted; insert the rest */                                             \
static void sort_q_##NAME##_insertion_from(TYPE *arr, size_t len, size_t sorted) {             \
    for (size_t i = sorted ? sorted : 1; i < len; i++) {                                       \
        TYPE key = arr[i];                                                                     \
        size_t j = i;                                                                          \
        while (j > 0 && arr[j - 1] > key) {                                                    \
            arr[j] = arr[j - 1];                                                               \
            j--;                                                                               \
        }                                                                                      \
        arr[j] = key;                                                                          \
    }                                                                                          \
}                                                                                              \
                                                                                               \
static void sort_q_##NAME##_reverse(TYPE *arr, size_t len) {                                   \
    if (len < 2) return;                                                                       \
    size_t i = 0, j = len - 1;                                                                 \
    while (i < j) {                                                                            \
        TYPE tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;                                      \
        i++; j--;                                                                              \
    }                                                                                          \
}                                                                                              \
                                                                                               \
/* Merge sorted ranges a and b into out (branchless) */                                          \
static void sort_q_##NAME##_merge_to(const TYPE *a, size_t na, const TYPE *b, size_t nb,       \
                                     TYPE *out) {                                              \
    size_t i = 0, j = 0, k = 0;                                                                \
    while (i < na && j < nb) {                                                                 \
        TYPE x = a[i], y = b[j];                                                               \
        int take_b = y < x;                                                                    \
        out[k++] = take_b ? y : x;                                                             \
        j += take_b;                                                                           \
        i += !take_b;                                                                          \
    }                                                                                          \
    if (i < na) memcpy(out + k, a + i, sizeof(TYPE) * (na - i));                               \
    if (j < nb && out + k != b + j) memmove(out + k, b + j, sizeof(TYPE) * (nb - j));         \
}                                                                                              \
                                                                                               \
/* Merge runs bnd[k0..k1) of arr recursively (depth-first, so small merges stay in cache).  \
 * Before merging two halves, the prefix of the left half that is already <= the right     \
 * half's minimum and the suffix of the right half that is already >= the left half's      \
 * maximum are trimmed by binary search; only the overlapping middle is moved. */           \
static void sort_q_##NAME##_merge_runs(TYPE *arr, const size_t *bnd, size_t k0, size_t k1,   \
                                       TYPE *buf) {                                          \
    if (k1 - k0 < 2) return;                                                                   \
    /* split at the run boundary nearest the element midpoint, so one huge run next to     \
     * many small ones is merged O(log) times, not once per level of run count */           \
    size_t target = bnd[k0] + (bnd[k1] - bnd[k0]) / 2, ka = k0 + 1, kb = k1 - 1;             \
    while (ka < kb) { size_t m = ka + (kb - ka) / 2; if (bnd[m] < target) ka = m + 1; else kb = m; } \
    size_t km = ka;                                                                            \
    if (km > k0 + 1 && target - bnd[km - 1] < bnd[km] - target) km--;                          \
    sort_q_##NAME##_merge_runs(arr, bnd, k0, km, buf);                                         \
    sort_q_##NAME##_merge_runs(arr, bnd, km, k1, buf);                                         \
    size_t lo = bnd[k0], mid = bnd[km], hi = bnd[k1];                                          \
    if (arr[mid - 1] <= arr[mid]) return;                                                      \
    /* first left element > arr[mid] */                                                        \
    TYPE rmin = arr[mid];                                                                      \
    size_t a = lo, b = mid;                                                                    \
    while (a < b) { size_t m = a + (b - a) / 2; if (arr[m] <= rmin) a = m + 1; else b = m; }   \
    lo = a;                                                                                    \
    /* first right element >= arr[mid - 1]; everything from there on is in place */            \
    TYPE lmax = arr[mid - 1];                                                                  \
    a = mid; b = hi;                                                                           \
    while (a < b) { size_t m = a + (b - a) / 2; if (arr[m] < lmax) a = m + 1; else b = m; }    \
    hi = a;                                                                                    \
    memcpy(buf, arr + lo, sizeof(TYPE) * (mid - lo));                                          \
    sort_q_##NAME##_merge_to(buf, mid - lo, arr + mid, hi - mid, arr + lo);                    \
}                                                                                              \
                                                                                               \
/* Natural merge: find run boundaries (slope change points) once, then merge runs only.      \
 * Cost O(n log r), r = number of runs; organ pipe (r=2) is effectively O(n).                \
 * Descending runs are non-strict: equal elements are indistinguishable, so reversing a    \
 * run with duplicates is still correct for plain integers. */                               \
static int sort_q_##NAME##_natural(TYPE *arr, size_t len) {                                    \
    size_t max_runs = len / SORT_Q_MINRUN + 2;                                                 \
    size_t *bnd = (size_t *)malloc(sizeof(size_t) * (max_runs + 1));                           \
    if (!bnd) return 0;                                                                        \
                                                                                               \
    size_t r = 0, i = 0;                                                                       \
    bnd[0] = 0;                                                                                \
    while (i < len) {                                                                          \
        size_t start = i, end = i + 1;                                                         \
        while (end < len && arr[end] == arr[start]) end++; /* equal prefix fits either way */ \
        if (end < len) {                                                                       \
            if (arr[end] < arr[start]) {                                                       \
                while (end < len && arr[end] <= arr[end - 1]) end++;                           \
                sort_q_##NAME##_reverse(arr + start, end - start);                             \
            } else {                                                                           \
                while (end < len && arr[end] >= arr[end - 1]) end++;                           \
            }                                                                                  \
        }                                                                                      \
        if (end - start < SORT_Q_MINRUN) {                                                     \
            size_t forced = start + SORT_Q_MINRUN < len ? start + SORT_Q_MINRUN : len;          \
            sort_q_##NAME##_insertion_from(arr + start, forced - start, end - start);           \
            end = forced;                                                                      \
        }                                                                                      \
        bnd[++r] = end;                                                                        \
        i = end;                                                                               \
    }                                                                                          \
    if (r > 1) {                                                                               \
        TYPE *buf = (TYPE *)malloc(sizeof(TYPE) * len);                                        \
        if (!buf) { free(bnd); return 0; }                                                     \
        sort_q_##NAME##_merge_runs(arr, bnd, 0, r, buf);                                       \
        free(buf);                                                                             \
    }                                                                                          \
    free(bnd);                                                                                 \
    return 1;                                                                                  \
}                                                                                              \
                                                                                               \
/* LSD radix on the offset key (x - min): only the bytes that actually vary are sorted,      \
 * and the sign bit needs no special handling. All histograms are built in one read pass. */ \
static int sort_q_##NAME##_radix(TYPE *arr, size_t len, TYPE mn, unsigned passes) {            \
    TYPE *output = (TYPE *)malloc(sizeof(TYPE) * len);                                         \
    if (!output) return 0;                                                                     \
    size_t hist[sizeof(TYPE)][256];                                                            \
    memset(hist, 0, sizeof(hist[0]) * passes);                                                 \
    for (size_t i = 0; i < len; i++) {                                                         \
        UTYPE key = (UTYPE)((UTYPE)arr[i] - (UTYPE)mn);                                        \
        for (unsigned p = 0; p < passes; p++) hist[p][(uint8_t)(key >> (8 * p))]++;            \
    }                                                                                          \
    TYPE *src = arr, *dst = output;                                                            \
    for (unsigned p = 0; p < passes; p++) {                                                    \
        size_t *bucket = hist[p];                                                              \
        UTYPE k0 = (UTYPE)((UTYPE)src[0] - (UTYPE)mn);                                         \
        /* skip the pass if every element shares this byte */                                 \
        if (bucket[(uint8_t)(k0 >> (8 * p))] == len) continue;                                 \
        size_t sum = 0;                                                                        \
        for (size_t b = 0; b < 256; b++) { size_t c = bucket[b]; bucket[b] = sum; sum += c; }  \
        for (size_t i = 0; i < len; i++) {                                                     \
            UTYPE key = (UTYPE)((UTYPE)src[i] - (UTYPE)mn);                                    \
            dst[bucket[(uint8_t)(key >> (8 * p))]++] = src[i];                                 \
        }                                                                                      \
        TYPE *tmp = src; src = dst; dst = tmp;                                                 \
    }                                                                                          \
    if (src != arr) memcpy(arr, src, sizeof(TYPE) * len);                                      \
    free(output);                                                                              \
    return 1;                                                                                  \
}                                                                                              \
                                                                                               \
/* Cherry-pick: pull out only the elements that break monotonicity, sort them, merge back from the end.                    \
 * On a violating x, both x and the last kept element (top) are picked, so a single                           \
 * oversized outlier cannot push every following element out.                                      \
 * If more than `limit` get picked, restore a valid permutation and return 0. */                            \
static int sort_q_##NAME##_cherry_pick(TYPE *arr, size_t len, size_t limit) {                  \
    TYPE *out = (TYPE *)malloc(sizeof(TYPE) * limit);                                          \
    if (!out) return 0;                                                                        \
    size_t k = 1, m = 0;                                                                       \
    for (size_t i = 1; i < len; i++) {                                                         \
        TYPE x = arr[i];                                                                       \
        if (k == 0 || x >= arr[k - 1]) {                                                       \
            arr[k++] = x;                                                                      \
        } else {                                                                               \
            if (m + 2 > limit) {                                                               \
                memcpy(arr + k, out, sizeof(TYPE) * m); /* gap arr[k..i) has size m */           \
                free(out);                                                                     \
                return 0;                                                                      \
            }                                                                                  \
            out[m++] = arr[--k];                                                               \
            out[m++] = x;                                                                      \
        }                                                                                      \
    }                                                                                          \
    sort_q_##NAME(out, m);                                                                     \
    /* merge arr[0..k) and out[0..m) in place, back to front */                                    \
    size_t w = len, i = k;                                                                     \
    while (m > 0) {                                                                            \
        if (i > 0 && arr[i - 1] > out[m - 1]) arr[--w] = arr[--i];                             \
        else                                  arr[--w] = out[--m];                             \
    }                                                                                          \
    free(out);                                                                                 \
    return 1;                                                                                  \
}                                                                                              \
                                                                                               \
/* In-place heapsort: used only when allocation fails, guarantees O(n log n) without memory. */ \
static void sort_q_##NAME##_sift(TYPE *arr, size_t root, size_t len) {                         \
    TYPE v = arr[root];                                                                        \
    for (;;) {                                                                                 \
        size_t c = 2 * root + 1;                                                               \
        if (c >= len) break;                                                                   \
        if (c + 1 < len && arr[c + 1] > arr[c]) c++;                                           \
        if (!(arr[c] > v)) break;                                                              \
        arr[root] = arr[c];                                                                    \
        root = c;                                                                              \
    }                                                                                          \
    arr[root] = v;                                                                             \
}                                                                                              \
static void sort_q_##NAME##_fallback(TYPE *arr, size_t len) {                                  \
    for (size_t i = len / 2; i-- > 0;) sort_q_##NAME##_sift(arr, i, len);                     \
    for (size_t end = len; end-- > 1;) {                                                       \
        TYPE t = arr[0]; arr[0] = arr[end]; arr[end] = t;                                      \
        sort_q_##NAME##_sift(arr, 0, end);                                                     \
    }                                                                                          \
}                                                                                              \
                                                                                               \
void sort_q_##NAME(TYPE *arr, size_t len) {                                                    \
    if (len < 2) return;                                                                       \
    if (len < SORT_Q_SMALL_THRESHOLD) {                                                        \
        sort_q_##NAME##_insertion_from(arr, len, 1);                                           \
        return;                                                                                \
    }                                                                                          \
                                                                                               \
    /* single vectorizable scan: descents, ascents, turning points (local min/max), min, max */ \
    size_t desc = (arr[1] < arr[0]), asc = (arr[1] > arr[0]), turns = 0;                       \
    TYPE mn = arr[0] < arr[1] ? arr[0] : arr[1];                                               \
    TYPE mx = arr[0] < arr[1] ? arr[1] : arr[0];                                               \
    for (size_t i = 1; i + 1 < len; i++) {                                                     \
        TYPE p = arr[i - 1], c = arr[i], n = arr[i + 1];                                       \
        desc  += (n < c);                                                                      \
        asc   += (n > c);                                                                      \
        turns += ((p < c) & (c > n)) | ((p > c) & (c < n));                                    \
        mn = n < mn ? n : mn;                                                                  \
        mx = n > mx ? n : mx;                                                                  \
    }                                                                                          \
                                                                                               \
    if (desc == 0) return;                                                                     \
    /* non-increasing (duplicates allowed): equal integers are indistinguishable,           \
     * so a plain reversal sorts it */                                                         \
    if (asc == 0) { sort_q_##NAME##_reverse(arr, len); return; }                               \
                                                                                               \
    /* nearly sorted: cherry-pick the outliers */                                              \
    if (desc <= len / SORT_Q_PICK_DIV && asc >= SORT_Q_PICK_ASC_RATIO * desc &&                \
        sort_q_##NAME##_cherry_pick(arr, len, len / SORT_Q_PICK_LIMIT_DIV + 2))                \
        return;                                                                                \
                                                                                               \
    /* Cost model (measured): one merge pass ~= 2 radix passes.                                \
     * natural merge: ~log2(runs) passes; radix: one pass per varying byte of (max - min). */  \
    UTYPE range = (UTYPE)((UTYPE)mx - (UTYPE)mn);                                              \
    unsigned passes = 0;                                                                       \
    while (range) { passes++; range = (UTYPE)(range >> 8); }                                   \
    if (passes == 0) return; /* all equal (unreachable: desc > 0) */                           \
    size_t runs = turns / 2 + 2;                                                               \
    int prefer_radix = len > SORT_Q_RADIX_THRESHOLD &&                                         \
        SORT_Q_MERGE_PASS_COST * sort_q_ceil_log2(runs) > passes;                              \
    if (prefer_radix && sort_q_##NAME##_radix(arr, len, mn, passes)) return;                   \
    if (sort_q_##NAME##_natural(arr, len)) return;                                             \
    sort_q_##NAME##_fallback(arr, len); /* malloc failure */                                    \
}

SORT_Q_DEFINE(i8,   int8_t,   uint8_t,  1)
SORT_Q_DEFINE(u8,   uint8_t,  uint8_t,  0)
SORT_Q_DEFINE(i16,  int16_t,  uint16_t, 1)
SORT_Q_DEFINE(u16,  uint16_t, uint16_t, 0)
SORT_Q_DEFINE(i32,  int32_t,  uint32_t, 1)
SORT_Q_DEFINE(u32,  uint32_t, uint32_t, 0)
SORT_Q_DEFINE(i64,  int64_t,  uint64_t, 1)
SORT_Q_DEFINE(u64,  uint64_t, uint64_t, 0)
SORT_Q_DEFINE(int,  int,      unsigned int, 1)
SORT_Q_DEFINE(uint, unsigned int, unsigned int, 0)
