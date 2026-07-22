/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_DATABASE_H
#define MS_DATABASE_H

#include "ms_common.h"

typedef struct ms_database ms_database_t;
typedef struct ms_scratch ms_scratch_t;

/**
 * @brief Create a database object.
 *
 * A database owns compiled matcher state and expression metadata.
 * It is the primary object for the external-memory runtime model.
 *
 * @param db: Output database pointer, should point to NULL before call.
 * @param type: Matcher type.
 * @param buckets: Main bucket count for HASH, 0 is allowed for AC.
 * @param secondary_buckets: Secondary bucket count for HASH, 0 is allowed for AC.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_create_database(ms_database_t **db, ms_matcher_type type,
                                    uint32_t buckets, uint32_t secondary_buckets);

/**
 * @brief Compile multiple expressions into a database.
 *
 * This is the simplified form of ms_compile_database_multi_ext() without
 * extended offset constraints.
 *
 * @param db: Output database pointer, should point to NULL before call.
 * @param type: Matcher type.
 * @param buckets: Main bucket count for HASH.
 * @param secondary_buckets: Secondary bucket count for HASH.
 * @param expressions: Pending expressions.
 * @param flags: Expression flags.
 * @param ids: Expression ids.
 * @param elements: Number of expressions.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_compile_database_multi(ms_database_t **db,
                                           ms_matcher_type type,
                                           uint32_t buckets,
                                           uint32_t secondary_buckets,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           uint32_t elements);

/**
 * @brief Compile multiple expressions with extended options into a database.
 *
 * Combination expressions are marked with MS_FLAG_COMBINATION.
 * HASH databases do not support combination rules.
 *
 * @param db: Output database pointer, should point to NULL before call.
 * @param type: Matcher type.
 * @param buckets: Main bucket count for HASH.
 * @param secondary_buckets: Secondary bucket count for HASH.
 * @param expressions: Pending expressions.
 * @param flags: Expression flags.
 * @param ids: Expression ids.
 * @param ext: Optional extended options for each expression.
 * @param elements: Number of expressions.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_compile_database_multi_ext(ms_database_t **db,
                                               ms_matcher_type type,
                                               uint32_t buckets,
                                               uint32_t secondary_buckets,
                                               const char *const *expressions,
                                               const uint32_t *flags,
                                               const uint32_t *ids,
                                               const ms_expr_ext_t *ext,
                                               uint32_t elements);

/**
 * @brief Free a database object.
 *
 * @param db: Database object created by ms_create_database() or
 *            ms_compile_database_multi*().
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_free_database(ms_database_t *db);

/**
 * @brief Allocate scratch workspace for block scan.
 *
 * Scratch owns the temporary runtime state for block scan, including logical
 * state vectors and AC hit capture buffers.
 *
 * @param db: Database object.
 * @param scratch: Output scratch pointer, should point to NULL before call.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_alloc_scratch(const ms_database_t *db, ms_scratch_t **scratch);

/**
 * @brief Free scratch workspace.
 *
 * @param scratch: Scratch object created by ms_alloc_scratch().
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_free_scratch(ms_scratch_t *scratch);

/**
 * @brief Scan one block with explicit database and scratch.
 *
 * Only AC databases support block scan. HASH databases remain fixed-key lookup
 * objects.
 *
 * @param db: Compiled AC database.
 * @param data: Pending matching data.
 * @param len: Length of pending matching data.
 * @param scratch: Scratch allocated from the same database.
 * @param ids: Output ids.
 * @param id_count: Output id count.
 * @param id_capacity: Capacity of @ids.
 * @return: 0 for success, MS_FIND_FAIL for no match, or an error code.
 */
int MS_CDECL ms_scan_database(const ms_database_t *db,
                                  const uint8_t *data,
                                  uint32_t len,
                                  ms_scratch_t *scratch,
                                  uint32_t *ids,
                                  uint32_t *id_count,
                                  uint32_t id_capacity);

/**
 * @brief Find one rule through a database object.
 *
 * HASH databases use fixed-key lookup. AC databases reuse the expression
 * runtime and follow the same reporting semantics as ms_scan_database().
 *
 * @param db: Database object.
 * @param rule: Pending matching rule. On success alias is updated.
 * @return: 0 for success, MS_FIND_FAIL for no match, or an error code.
 */
int MS_CDECL ms_find_database_rule(const ms_database_t *db, ms_rule_t *rule);

/**
 * @brief Remove rules from a compiled AC database.
 *
 * Plain rules are identified by id, key and key length. Combination rules are
 * identified by id and use a NULL key with key length zero. Removing a plain
 * rule also removes combinations that depend on it, matching ms_remove_rule()
 * semantics. A multi-rule request is atomic: either every requested removal
 * is committed, or the database remains unchanged.
 *
 * Existing streams become stale and return MS_STREAM_STALE until reset with
 * ms_reset_stream(). Existing scratch objects are refreshed automatically on
 * their next scan. Callers must externally synchronize removal with scans.
 * HASH databases are not supported by this API.
 */
int MS_CDECL ms_remove_database_rule(ms_database_t *db,
                                      const ms_rule_t *rule,
                                      uint32_t rule_num);

/**
 * @brief Open a stream from a database.
 *
 * Only AC databases support stream mode. Stream object and persistent stream
 * state are allocated from the stream allocator.
 *
 * @param db: Compiled AC database.
 * @param stream: Output stream pointer, should point to NULL before call.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MS_CDECL ms_open_stream(const ms_database_t *db, ms_stream_t **stream);

/**
 * @brief Scan one chunk on a database stream with explicit scratch.
 *
 * Stream owns cross-chunk state. Per-call capture buffers, state copies and
 * logical evaluation workspace come from scratch.
 */
int MS_CDECL ms_scan_stream(ms_stream_t *stream,
                                const uint8_t *data,
                                uint32_t len,
                                ms_scratch_t *scratch,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity);

/**
 * @brief Flush a stream at end of data without closing it.
 */
int MS_CDECL ms_flush_stream(ms_stream_t *stream,
                                 ms_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity);

/**
 * @brief Flush a stream at end of data and close it.
 *
 * After parameter validation succeeds, the stream is released even if the
 * end-of-data evaluation reports no ids.
 */
int MS_CDECL ms_close_stream(ms_stream_t *stream,
                                 ms_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity);

#endif
