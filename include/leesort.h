#ifndef LEESORT_H
#define LEESORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sort_q_i8  (int8_t       *arr, size_t len);
void sort_q_u8  (uint8_t      *arr, size_t len);
void sort_q_i16 (int16_t      *arr, size_t len);
void sort_q_u16 (uint16_t     *arr, size_t len);
void sort_q_i32 (int32_t      *arr, size_t len);
void sort_q_u32 (uint32_t     *arr, size_t len);
void sort_q_i64 (int64_t      *arr, size_t len);
void sort_q_u64 (uint64_t     *arr, size_t len);
void sort_q_int (int          *arr, size_t len);
void sort_q_uint(unsigned int *arr, size_t len);

#ifdef __cplusplus
}
#endif

/* C11 _Generic dispatch. Unsupported pointer types are a compile error
 * (the old `default: sort_q_int` silently sorted e.g. long long* with the wrong element size). */
#if !defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define sort_q(arr, len) _Generic((arr), \
    int8_t*:   sort_q_i8,                \
    uint8_t*:  sort_q_u8,                \
    int16_t*:  sort_q_i16,               \
    uint16_t*: sort_q_u16,               \
    int32_t*:  sort_q_i32,               \
    uint32_t*: sort_q_u32,               \
    int64_t*:  sort_q_i64,               \
    uint64_t*: sort_q_u64                \
)(arr, len)
#endif

#endif /* LEESORT_H */
