/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_DATABASE_H
#define MMPM_DATABASE_H

#include "mmpm_common.h"

typedef struct mmpm_database mmpm_database_t;
typedef struct mmpm_scratch mmpm_scratch_t;

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
int MMPM_CDECL mmpm_create_database(mmpm_database_t **db, mmpm_matcher_type type,
                                    uint32_t buckets, uint32_t secondary_buckets);

/**
 * @brief Compile multiple expressions into a database.
 *
 * This is the simplified form of mmpm_compile_database_multi_ext() without
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
int MMPM_CDECL mmpm_compile_database_multi(mmpm_database_t **db,
                                           mmpm_matcher_type type,
                                           uint32_t buckets,
                                           uint32_t secondary_buckets,
                                           const char *const *expressions,
                                           const uint32_t *flags,
                                           const uint32_t *ids,
                                           uint32_t elements);

/**
 * @brief Compile multiple expressions with extended options into a database.
 *
 * Combination expressions are marked with MMPM_FLAG_COMBINATION.
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
int MMPM_CDECL mmpm_compile_database_multi_ext(mmpm_database_t **db,
                                               mmpm_matcher_type type,
                                               uint32_t buckets,
                                               uint32_t secondary_buckets,
                                               const char *const *expressions,
                                               const uint32_t *flags,
                                               const uint32_t *ids,
                                               const mmpm_expr_ext_t *ext,
                                               uint32_t elements);

/**
 * @brief Free a database object.
 *
 * @param db: Database object created by mmpm_create_database() or
 *            mmpm_compile_database_multi*().
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MMPM_CDECL mmpm_free_database(mmpm_database_t *db);

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
int MMPM_CDECL mmpm_alloc_scratch(const mmpm_database_t *db, mmpm_scratch_t **scratch);

/**
 * @brief Free scratch workspace.
 *
 * @param scratch: Scratch object created by mmpm_alloc_scratch().
 * @return: 0 for success, an error code(negative number) for failure.
 */
int MMPM_CDECL mmpm_free_scratch(mmpm_scratch_t *scratch);

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
 * @return: 0 for success, MMPM_FIND_FAIL for no match, or an error code.
 */
int MMPM_CDECL mmpm_scan_database(const mmpm_database_t *db,
                                  const uint8_t *data,
                                  uint32_t len,
                                  mmpm_scratch_t *scratch,
                                  uint32_t *ids,
                                  uint32_t *id_count,
                                  uint32_t id_capacity);

/**
 * @brief Find one rule through a database object.
 *
 * HASH databases use fixed-key lookup. AC databases reuse the expression
 * runtime and follow the same reporting semantics as mmpm_scan_database().
 *
 * @param db: Database object.
 * @param rule: Pending matching rule. On success alias is updated.
 * @return: 0 for success, MMPM_FIND_FAIL for no match, or an error code.
 */
int MMPM_CDECL mmpm_find_database_rule(const mmpm_database_t *db, mmpm_rule_t *rule);

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
int MMPM_CDECL mmpm_open_stream(const mmpm_database_t *db, ms_stream_t **stream);

/**
 * @brief Scan one chunk on a database stream with explicit scratch.
 *
 * Stream owns cross-chunk state. Per-call capture buffers, state copies and
 * logical evaluation workspace come from scratch.
 */
int MMPM_CDECL mmpm_scan_stream(ms_stream_t *stream,
                                const uint8_t *data,
                                uint32_t len,
                                mmpm_scratch_t *scratch,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity);

/**
 * @brief Flush a stream at end of data without closing it.
 */
int MMPM_CDECL mmpm_flush_stream(ms_stream_t *stream,
                                 mmpm_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity);

/**
 * @brief Flush a stream at end of data and close it.
 *
 * After parameter validation succeeds, the stream is released even if the
 * end-of-data evaluation reports no ids.
 */
int MMPM_CDECL mmpm_close_stream(ms_stream_t *stream,
                                 mmpm_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity);

#endif
