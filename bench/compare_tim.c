/* TimSort baseline wrappers (swenson/sort is a C macro library). */
#include <stddef.h>
#include <stdint.h>

#define SORT_NAME tim_i32
#define SORT_TYPE int32_t
#include "sort.h"
#undef SORT_NAME
#undef SORT_TYPE

#define SORT_NAME tim_u64
#define SORT_TYPE uint64_t
#include "sort.h"

void baseline_timsort_i32(int32_t *a, size_t n) { tim_i32_tim_sort(a, n); }
void baseline_timsort_u64(uint64_t *a, size_t n) { tim_u64_tim_sort(a, n); }
