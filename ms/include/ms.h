/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_H
#define MS_H

#include <stdint.h>
#include <stdbool.h>
#include "ms_allocator.h"

#define MS_SUCCESS            0
#define MS_INVALID_PARA       (-1)
#define MS_MALLOC_FAIL        (-2)
#define MS_BUILT_ALREADY      (-3)
#define MS_FIND_FAIL          (-4)
#define MS_INIT_RWLOCK_FAIL   (-5)
#define MS_MEMCPY_FAIL        (-6)
#define MS_NOT_SUPPORTED      (-7)
#define MS_STREAM_STALE       (-8)

typedef uint32_t (*hash_calc_func)(const uint8_t *key, uint32_t key_len);

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
    void *ms_matcher;
    ms_matcher_type type;
    
    /* mark whether the ac automation has been built. */
    bool is_built;
} ms_handler_t;

typedef struct ms_stream ms_stream_t;
/**
 * Initialize multi-pattern matching handler.
 *
 * @handler: Matching engine handle.
 * @type: Types of Matching Engines, ac or hash.
 * @bucket_num: Number of primary hash buckets.
 * @secondary_buckets_num: Number of secondary hash buckets.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_handler_init(ms_handler_t *handler, ms_matcher_type type, uint32_t buckets, uint32_t secondary_buckets);

/**
 * Register hash calculation function.
 *
 * @handler: Matching engine handle.
 * @hash_func: Hash function pointer.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_set_hash_func(ms_handler_t *handler, hash_calc_func hash_func);

/**
 * Import rules.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be imported.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_add_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num);

/**
 * Delete target rules.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be deleted.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_remove_rule(ms_handler_t *handler, ms_rule_t *rule, uint32_t rule_num);

/**
 * Matching target rule.
 *
 * @handler: Matching engine handle.
 * @rule: Pending matching rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_find_rule(ms_handler_t *handler, ms_rule_t *rule);

/**
 * Open a stream for AC matching.
 *
 * @handler: Matching engine handle.
 * @stream: Stream handle to be opened.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_open_stream(ms_handler_t *handler, ms_stream_t **stream);

/**
 * Scan data on an opened AC stream.
 *
 * @stream: Opened stream handle.
 * @data: Pending matching data.
 * @len: Length of pending matching data.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_scan_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len);

/**
 * Reset an opened AC stream to initial state.
 *
 * @stream: Opened stream handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_reset_stream(ms_stream_t *stream);

/**
 * Close an opened AC stream.
 *
 * @stream: Opened stream handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_close_stream(ms_stream_t *stream);

/**
 * Destroy multi-pattern matching handler.
 *
 * @handler: Matching engine handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_handler_destroy(ms_handler_t *handler);

/**
 * Set global memory allocator for MS.
 *
 * @alloc_func: Memory allocation function pointer.
 * @calloc_func: Memory allocation and initialization function pointer.
 * @free_func: Memory deallocation function pointer.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_set_allocator(ms_alloc_t alloc_func, ms_calloc_t calloc_func, ms_free_t free_func);

#endif
