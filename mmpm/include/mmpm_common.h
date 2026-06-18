/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_COMMON_H
#define MMPM_COMMON_H

#include <stdbool.h>
#include <stdint.h>

#include "mmpm_allocator.h"

#define MMPM_SUCCESS            0
#define MMPM_INVALID_PARA       (-1)
#define MMPM_MALLOC_FAIL        (-2)
#define MMPM_BUILT_ALREADY      (-3)
#define MMPM_FIND_FAIL          (-4)
#define MMPM_INIT_RWLOCK_FAIL   (-5)
#define MMPM_MEMCPY_FAIL        (-6)
#define MMPM_NOT_SUPPORTED      (-7)
#define MMPM_STREAM_STALE       (-8)

#define MMPM_FLAG_NONE          0u
#define MMPM_FLAG_COMBINATION   1u
#define MMPM_FLAG_QUIET         2u
#define MMPM_FLAG_SINGLEMATCH   4u

#define MMPM_EXT_FLAG_MIN_OFFSET 1u
#define MMPM_EXT_FLAG_MAX_OFFSET 2u

typedef uint32_t (*hash_calc_func)(const uint8_t *key, uint32_t key_len);

typedef enum {
    MMPM_MATCHER_NONE = 0,
    MMPM_MATCHER_TYPE_HASH,
    MMPM_MATCHER_TYPE_AC,
    MMPM_MATCHER_TYPE_MAX
} mmpm_matcher_type;

typedef struct {
    uint32_t alias;
    uint32_t key_len;
    uint8_t *key;
} mmpm_rule_t;

typedef struct {
    uint32_t flags;
    uint64_t min_offset;
    uint64_t max_offset;
} mmpm_expr_ext_t;

/**
 * Legacy aggregate handle.
 *
 * This object predates the database/scratch/stream split and bundles the
 * matcher instance together with the compiled expression context. It remains
 * useful for compatibility APIs, mutable rule management, and some internal
 * bridging code, but new public scan/stream paths should prefer
 * mmpm_database_t, mmpm_scratch_t and ms_stream_t.
 */
typedef struct {
    void *mmpm_matcher;
    void *mmpm_compile_ctx;
    mmpm_matcher_type type;

    /* mark whether the ac automation has been built. */
    bool is_built;
} mmpm_handler_t;

typedef struct ms_stream ms_stream_t;

/**
 * Initialize multi-pattern matching handler.
 *
 * Compatibility note: this API initializes the legacy aggregate handle.
 * New code that uses external-memory lifecycle management should prefer the
 * database APIs.
 *
 * @handler: Matching engine handle.
 * @type: Types of Matching Engines, ac or hash.
 * @bucket_num: Number of primary hash buckets.
 * @secondary_buckets_num: Number of secondary hash buckets.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_handler_init(mmpm_handler_t *handler, mmpm_matcher_type type, uint32_t buckets, uint32_t secondary_buckets);

/**
 * Destroy multi-pattern matching handler.
 *
 * Compatibility note: this API destroys the legacy aggregate handle.
 * New code that uses external-memory lifecycle management should prefer the
 * database APIs.
 *
 * @handler: Matching engine handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_handler_destroy(mmpm_handler_t *handler);

/**
 * Set global memory allocator for MMPM.
 *
 * @alloc_func: Memory allocation function pointer.
 * @calloc_func: Memory allocation and initialization function pointer.
 * @free_func: Memory deallocation function pointer.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MMPM_CDECL mmpm_set_allocator(mmpm_alloc_t alloc_func, mmpm_calloc_t calloc_func, mmpm_free_t free_func);

#endif
