/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_COMPILE_H
#define MS_COMPILE_H

#include "ms_handler.h"

/**
 * Compile multiple expression-style rules.
 *
 * Combination rules marked with MS_FLAG_QUIET are accepted and skipped:
 * they are not compiled as reported combinations and are not added as plain
 * fixed-string rules.
 *
 * Compatibility note: database-style callers should prefer
 * ms_compile_database_multi() or ms_compile_database_multi_ext().
 *
 * @handler: Matching engine handle.
 * @expressions: NULL-terminated expressions to be compiled.
 * @flags: Flags associated with each expression. NULL means all flags are zero.
 * @ids: IDs associated with each expression. NULL means all IDs are zero.
 * @elements: Number of expressions.
 * @return: 0 for success, an error code(negative number) for failure.
 */
int ms_compile_multi(ms_handler_t *handler, const char *const *expressions,
                     const uint32_t *flags, const uint32_t *ids,
                     uint32_t elements);

/**
 * Compile multiple expression-style rules with extended options.
 *
 * Offset bounds apply to combination rules. Megascan is fixed-string based, so
 * bounds are checked against the fixed-string match end offset that makes the
 * combination true. In stream mode, that offset is cumulative across chunks.
 *
 * Compatibility note: database-style callers should prefer
 * ms_compile_database_multi_ext().
 */
int ms_compile_multi_ext(ms_handler_t *handler, const char *const *expressions,
                         const uint32_t *flags, const uint32_t *ids,
                         const ms_expr_ext_t *ext, uint32_t elements);

/**
 * Import expression-style rules.
 *
 * Compatibility note: this legacy helper stays on the mutable handler path.
 */
int ms_add_exprs(ms_handler_t *handler, const char *const *expr,
                 const uint32_t *ids, const uint32_t *flags, uint32_t elements);

#endif
