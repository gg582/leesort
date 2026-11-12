/* leesort-bench CLI: leesort-bench [N ...] */
#include "leesort_bench.h"
#include <stdlib.h>

int main(int argc, char **argv) {
    size_t *ns = NULL, cnt = 0;
    if (argc > 1) {
        cnt = (size_t)argc - 1;
        ns = malloc(sizeof(size_t) * cnt);
        for (size_t i = 0; i < cnt; i++) ns[i] = strtoull(argv[i + 1], NULL, 10);
    }
    int fails = leesort_bench(stdout, ns, cnt, LEESORT_BENCH_ALL);
    free(ns);
    return fails != 0;
}
