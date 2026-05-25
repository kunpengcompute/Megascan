/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_H
#define MMPM_H

#include <stdint.h>
#include <stdbool.h>
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
    void *mmpm_matcher;
    mmpm_matcher_type type;
    
    /* mark whether the ac automation has been built. */
    bool is_built;
} mmpm_handler_t;

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
int mmpm_handler_init(mmpm_handler_t *handler, mmpm_matcher_type type, uint32_t buckets, uint32_t secondary_buckets);

/**
 * Register hash calculation function.
 *
 * @handler: Matching engine handle.
 * @hash_func: Hash function pointer.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_set_hash_func(mmpm_handler_t *handler, hash_calc_func hash_func);

/**
 * Import rules.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be imported.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_add_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);

/**
 * Delete target rules.
 *
 * @handler: Matching engine handle.
 * @rule: Rules to be deleted.
 * @rule_num: Number of rules, needs to be consistent with the parameter @rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_remove_rule(mmpm_handler_t *handler, mmpm_rule_t *rule, uint32_t rule_num);

/**
 * Matching target rule.
 *
 * @handler: Matching engine handle.
 * @rule: Pending matching rule.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_find_rule(mmpm_handler_t *handler, mmpm_rule_t *rule);

/**
 * Open a stream for AC matching.
 *
 * @handler: Matching engine handle.
 * @stream: Stream handle to be opened.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_open_stream(mmpm_handler_t *handler, ms_stream_t **stream);

/**
 * Scan data on an opened AC stream.
 *
 * @stream: Opened stream handle.
 * @data: Pending matching data.
 * @len: Length of pending matching data.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_scan_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len);

/**
 * Reset an opened AC stream to initial state.
 *
 * @stream: Opened stream handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_reset_stream(ms_stream_t *stream);

/**
 * Close an opened AC stream.
 *
 * @stream: Opened stream handle.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_close_stream(ms_stream_t *stream);

/**
 * Destroy multi-pattern matching handler.
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
