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
 * Compile multiple expression-style rules.
 *
 * @handler: Matching engine handle.
 * @expressions: NULL-terminated expressions to be compiled.
 * @flags: Flags associated with each expression. NULL means all flags are zero.
 * @ids: IDs associated with each expression. NULL means all IDs are zero.
 * @elements: Number of expressions.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int mmpm_compile_multi(mmpm_handler_t *handler, const char *const *expressions, const uint32_t *flags,
                       const uint32_t *ids, uint32_t elements);

/**
 * Compile multiple expression-style rules with extended options.
 *
 * Offset bounds apply to combination rules and use the scan-call end offset.
 * In stream mode, the offset is cumulative across chunks.
 */
int mmpm_compile_multi_ext(mmpm_handler_t *handler,
                           const char *const *expressions,
                           const uint32_t *flags,
                           const uint32_t *ids,
                           const mmpm_expr_ext_t *ext,
                           uint32_t elements);

/**
 * Import expression-style rules.
 *
 * This is a compatibility wrapper around mmpm_compile_multi().
 */
int mmpm_add_exprs(mmpm_handler_t *handler, const char *const *expr, const uint32_t *ids,
                   const uint32_t *flags, uint32_t elements);

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
 * Scan data with expression-style rules.
 *
 * Non-quiet plain rules and matched combination rules are written to @ids.
 * Quiet plain rules participate in combination evaluation but are not written.
 * If @id_capacity is too small, @id_count is set to the required count and
 * MMPM_INVALID_PARA is returned.
 *
 * @handler: Matching engine handle. Only AC handlers are supported currently.
 * @data: Pending matching data.
 * @len: Length of pending matching data.
 * @ids: Output IDs. May be NULL only when @id_capacity is zero.
 * @id_count: Output ID count or required count on capacity failure.
 * @id_capacity: Capacity of @ids.
 * @return: 0 for success, MMPM_FIND_FAIL for no reported IDs, or an error code.
 */
int mmpm_scan_expr(mmpm_handler_t *handler, const uint8_t *data, uint32_t len,
                   uint32_t *ids, uint32_t *id_count, uint32_t id_capacity);

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
 * Scan data on an opened AC stream with expression-style reporting.
 *
 * Plain rule state and combination state are accumulated across chunks for the
 * lifetime of the stream until reset. This API reports only the IDs newly
 * produced by the current chunk.
 *
 * @stream: Opened stream handle.
 * @data: Pending matching data.
 * @len: Length of pending matching data.
 * @ids: Output IDs. May be NULL only when @id_capacity is zero.
 * @id_count: Output ID count or required count on capacity failure.
 * @id_capacity: Capacity of @ids.
 * @return: 0 for success, MMPM_FIND_FAIL for no reported IDs, or an error code.
 */
int mmpm_scan_expr_stream(ms_stream_t *stream, const uint8_t *data, uint32_t len,
                          uint32_t *ids, uint32_t *id_count, uint32_t id_capacity);

/**
 * Flush an expression stream at end of data.
 *
 * This reports combinations that can match without a positive sub-expression
 * hit in the current chunk, such as !101, using the stream's accumulated
 * logical state.
 */
int mmpm_scan_expr_stream_flush(ms_stream_t *stream,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity);

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
