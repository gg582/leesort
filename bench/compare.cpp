// leesort vs. established sorting implementations.
// usage: leesort-compare [N ...]     (default: 100000 1000000 10000000)
// Output: one CSV line per (type, N, distribution, algorithm), then a summary table.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include "leesort.h"
#include "pdqsort.h"
#include "ska_sort.hpp"
#include "x86simdsort-static-incl.h"

extern "C" void baseline_timsort_i32(int32_t *a, size_t n);
extern "C" void baseline_timsort_u64(uint64_t *a, size_t n);

static uint64_t rng_state;
static uint64_t rnd() {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Plain LSD radix: one histogram pass, skips trivial byte passes, no adaptivity.
template <class T> static void radix_plain(T *a, size_t n) {
    using U = std::make_unsigned_t<T>;
    constexpr int B = sizeof(T);
    const U flip = std::is_signed_v<T> ? (U)((U)1 << (8 * B - 1)) : 0;
    std::vector<T> tmp(n);
    static size_t h[B][256];
    std::memset(h, 0, sizeof h);
    for (size_t i = 0; i < n; i++) {
        U k = (U)a[i] ^ flip;
        for (int p = 0; p < B; p++) h[p][(k >> (8 * p)) & 0xFF]++;
    }
    T *src = a, *dst = tmp.data();
    for (int p = 0; p < B; p++) {
        if (h[p][(((U)src[0] ^ flip) >> (8 * p)) & 0xFF] == n) continue;
        size_t s = 0;
        for (int b = 0; b < 256; b++) { size_t c = h[p][b]; h[p][b] = s; s += c; }
        for (size_t i = 0; i < n; i++) {
            U k = (U)src[i] ^ flip;
            dst[h[p][(k >> (8 * p)) & 0xFF]++] = src[i];
        }
        std::swap(src, dst);
    }
    if (src != a) std::memcpy(a, src, n * sizeof(T));
}

template <class T> static int cmp_c(const void *x, const void *y) {
    T a = *(const T *)x, b = *(const T *)y;
    return (a > b) - (a < b);
}

static const char *DISTS[] = {
    "random", "sorted", "reversed", "nearly_0.1%", "nearly_1%", "nearly_10%",
    "few_unique", "sawtooth", "organ_pipe", "sorted+tail5%", "jitter",
    "k_sorted_64", "mixed_blocks",
};
static const int NDIST = sizeof DISTS / sizeof *DISTS;

template <class T> static void fill(std::vector<T> &a, int d) {
    size_t n = a.size();
    rng_state = 777 + (uint64_t)d * 1000003;
    auto swaps = [&](size_t cnt) {
        for (size_t k = 0; k < cnt; k++) std::swap(a[rnd() % n], a[rnd() % n]);
    };
    switch (d) {
    case 0: for (auto &x : a) x = (T)rnd(); break;
    case 1: for (size_t i = 0; i < n; i++) a[i] = (T)i; break;
    case 2: for (size_t i = 0; i < n; i++) a[i] = (T)(n - i); break;
    case 3: for (size_t i = 0; i < n; i++) a[i] = (T)i; swaps(n / 1000); break;
    case 4: for (size_t i = 0; i < n; i++) a[i] = (T)i; swaps(n / 100); break;
    case 5: for (size_t i = 0; i < n; i++) a[i] = (T)i; swaps(n / 10); break;
    case 6: for (auto &x : a) x = (T)(rnd() % 16); break;
    case 7: for (size_t i = 0; i < n; i++) a[i] = (T)(i % 1000); break;
    case 8: for (size_t i = 0; i < n; i++) a[i] = (T)(i < n / 2 ? i : n - i); break;
    case 9: // sorted log with a randomly-valued appended tail
        for (size_t i = 0; i < n; i++) a[i] = i < n - n / 20 ? (T)(i * 4) : (T)(rnd() % (4 * n));
        break;
    case 10: // timestamps with bounded local disorder
        for (size_t i = 0; i < n; i++) a[i] = (T)(i * 16 + rnd() % 256);
        break;
    case 11: { // concatenation of 64 independently sorted random lists
        for (auto &x : a) x = (T)(rnd() % (8 * n));
        size_t blk = (n + 63) / 64;
        for (size_t s = 0; s < n; s += blk) std::sort(a.begin() + s, a.begin() + std::min(n, s + blk));
        break;
    }
    case 12: { // 64K blocks, each sorted, reversed or random
        for (auto &x : a) x = (T)(rnd() % (8 * n));
        const size_t blk = 65536;
        for (size_t s = 0; s < n; s += blk) {
            auto b = a.begin() + s, e = a.begin() + std::min(n, s + blk);
            switch (rnd() % 3) {
            case 0: std::sort(b, e); break;
            case 1: std::sort(b, e, std::greater<T>()); break;
            default: break;
            }
        }
        break;
    }
    }
}

struct Algo { const char *name; };
static const char *ALGOS[] = {
    "leesort", "radix_plain", "ska_sort", "x86simd", "pdqsort_bl",
    "std_sort", "stable_sort", "timsort", "qsort",
};
static const int NALGO = sizeof ALGOS / sizeof *ALGOS;

template <class T> static void run_algo(int k, T *a, size_t n);
template <> void run_algo<int32_t>(int k, int32_t *a, size_t n) {
    switch (k) {
    case 0: sort_q_i32(a, n); break;
    case 1: radix_plain(a, n); break;
    case 2: ska_sort(a, a + n); break;
    case 3: x86simdsortStatic::qsort(a, n); break;
    case 4: pdqsort_branchless(a, a + n); break;
    case 5: std::sort(a, a + n); break;
    case 6: std::stable_sort(a, a + n); break;
    case 7: baseline_timsort_i32(a, n); break;
    case 8: std::qsort(a, n, sizeof *a, cmp_c<int32_t>); break;
    }
}
template <> void run_algo<uint64_t>(int k, uint64_t *a, size_t n) {
    switch (k) {
    case 0: sort_q_u64(a, n); break;
    case 1: radix_plain(a, n); break;
    case 2: ska_sort(a, a + n); break;
    case 3: x86simdsortStatic::qsort(a, n); break;
    case 4: pdqsort_branchless(a, a + n); break;
    case 5: std::sort(a, a + n); break;
    case 6: std::stable_sort(a, a + n); break;
    case 7: baseline_timsort_u64(a, n); break;
    case 8: std::qsort(a, n, sizeof *a, cmp_c<uint64_t>); break;
    }
}

static double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static int fails = 0;
template <class T> static void bench(const char *tname, size_t n) {
    std::vector<T> src(n), w(n), ref(n);
    int reps = (int)std::clamp<size_t>(30000000 / (n + 1), 3, 200);
    for (int d = 0; d < NDIST; d++) {
        fill(src, d);
        ref = src;
        std::sort(ref.begin(), ref.end());
        for (int k = 0; k < NALGO; k++) {
            double best = 1e30;
            for (int r = 0; r < reps; r++) {
                std::memcpy(w.data(), src.data(), n * sizeof(T));
                double t0 = now();
                run_algo<T>(k, w.data(), n);
                double t = now() - t0;
                if (t < best) best = t;
            }
            bool ok = std::memcmp(w.data(), ref.data(), n * sizeof(T)) == 0;
            fails += !ok;
            std::printf("%s,%zu,%s,%s,%.4f%s\n", tname, n, DISTS[d], ALGOS[k], best * 1e3,
                        ok ? "" : ",MISMATCH");
            std::fflush(stdout);
        }
    }
}

int main(int argc, char **argv) {
    std::vector<size_t> ns = {100000, 1000000, 10000000};
    if (argc > 1) {
        ns.clear();
        for (int i = 1; i < argc; i++) ns.push_back(std::strtoull(argv[i], nullptr, 10));
    }
    std::printf("type,n,dist,algo,ms\n");
    for (size_t n : ns) bench<int32_t>("i32", n);
    for (size_t n : ns) bench<uint64_t>("u64", n);
    std::fprintf(stderr, fails ? "%d MISMATCHES\n" : "all outputs verified\n", fails);
    return fails != 0;
}
