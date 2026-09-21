/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_COMMON_H
#define MS_COMMON_H

#include <stdbool.h>
#include <stdint.h>

#include "ms_allocator.h"

#define MS_SUCCESS 0
#define MS_INVALID_PARA (-1)
#define MS_MALLOC_FAIL (-2)
#define MS_BUILT_ALREADY (-3)
#define MS_FIND_FAIL (-4)
#define MS_INIT_RWLOCK_FAIL (-5)
#define MS_MEMCPY_FAIL (-6)
#define MS_NOT_SUPPORTED (-7)
#define MS_STREAM_STALE (-8)

#define MS_FLAG_NONE 0u
#define MS_FLAG_COMBINATION 1u
#define MS_FLAG_QUIET 2u
#define MS_FLAG_SINGLEMATCH 4u

#define MS_EXT_FLAG_MIN_OFFSET 1u
#define MS_EXT_FLAG_MAX_OFFSET 2u

typedef uint32_t (*hash_calc_func)(const uint8_t *key, uint32_t key_len);

typedef struct {
  uint32_t id;
  uint64_t from;
  uint64_t to;
} ms_match_event_t;

typedef enum {
  MS_MATCHER_NONE = 0,
  MS_MATCHER_TYPE_HASH,
  MS_MATCHER_TYPE_AC,
  MS_MATCHER_TYPE_MAX
} ms_matcher_type;

typedef struct {
  uint32_t alias;
  uint32_t key_len;
  uint8_t *key;
} ms_rule_t;

typedef struct {
  uint32_t flags;
  uint64_t min_offset;
  uint64_t max_offset;
} ms_expr_ext_t;

typedef struct ms_stream ms_stream_t;

/**
 * Set global memory allocator for MS.
 *
 * @alloc_func: Memory allocation function pointer.
 * @calloc_func: Memory allocation and initialization function pointer.
 * @free_func: Memory deallocation function pointer.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_set_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func,
                     ms_free_t free_func);

#endif
