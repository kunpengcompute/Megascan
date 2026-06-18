/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_COMPILE_H
#define MMPM_COMPILE_H

#include "mmpm_common.h"

/**
 * Compile multiple expression-style rules.
 *
 * This is the legacy aggregate-handle import entry. New code that wants one
 * stable recommended path should prefer mmpm_compile_database_multi() or
 * mmpm_compile_database_multi_ext().
 *
 * Combination rules marked with MMPM_FLAG_QUIET are accepted and skipped:
 * they are not compiled as reported combinations and are not added as plain
 * fixed-string rules.
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
 * This is the legacy aggregate-handle import entry with extended options.
 * New code should prefer mmpm_compile_database_multi_ext().
 *
 * Offset bounds apply to combination rules. Megascan is fixed-string based, so
 * bounds are checked against the fixed-string match end offset that makes the
 * combination true. In stream mode, that offset is cumulative across chunks.
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
 * This is a compatibility wrapper around mmpm_compile_multi() and therefore
 * also stays on the mutable handler path. New code should prefer
 * mmpm_compile_database_multi() or mmpm_compile_database_multi_ext().
 */
int mmpm_add_exprs(mmpm_handler_t *handler, const char *const *expr, const uint32_t *ids,
                   const uint32_t *flags, uint32_t elements);

#endif
