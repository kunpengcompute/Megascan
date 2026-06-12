/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MMPM_EXPR_INTERNAL_H
#define MMPM_EXPR_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "mmpm.h"
#include "mmpm_ac.h"
#include "mmpm_logical.h"

typedef struct {
    uint32_t id;
    uint32_t flags;
    uint32_t lkey;
} mmpm_compile_expr_t;

typedef struct {
    uint32_t id;
    uint32_t result;
    mmpm_logical_tree_t tree;
} mmpm_compile_comb_t;

typedef struct {
    mmpm_compile_expr_t *exprs;
    uint32_t expr_count;
    uint32_t expr_capacity;
    mmpm_compile_comb_t *combs;
    uint32_t comb_count;
    uint32_t comb_capacity;
    uint32_t logical_count;
} mmpm_compile_context_t;

typedef struct {
    uint32_t *aliases;
    uint32_t count;
    uint32_t capacity;
} mmpm_compile_alias_capture_t;

bool mmpm_compile_is_combination(const uint32_t *flags, uint32_t index);
bool mmpm_compile_context_has_id(const mmpm_compile_context_t *ctx, uint32_t id);
int mmpm_compile_context_find_lkey(const mmpm_compile_context_t *ctx,
                                   uint32_t id,
                                   uint32_t *lkey);
const mmpm_compile_expr_t *mmpm_compile_context_find_expr(const mmpm_compile_context_t *ctx,
                                                          uint32_t id);
uint32_t mmpm_compile_logical_byte_count(uint32_t logical_count);
int mmpm_compile_mark_aliases(const mmpm_compile_context_t *ctx,
                              const uint32_t *aliases,
                              uint32_t alias_count,
                              uint8_t *logical_vector,
                              uint32_t logical_count);
void mmpm_compile_context_free(mmpm_compile_context_t *ctx);
int mmpm_compile_alias_capture_reserve(mmpm_compile_alias_capture_t *capture);
int mmpm_compile_capture_alias(uint32_t alias, void *ctx);
int mmpm_compile_capture_alias_unique(uint32_t alias, void *ctx);

int mmpm_compile_stream_state_init(mmpm_handler_t *handler,
                                   uint8_t **logical_vector,
                                   uint32_t *logical_count,
                                   uint8_t **combination_vector,
                                   uint32_t *combination_count);
void mmpm_compile_stream_state_destroy(uint8_t *logical_vector,
                                       uint8_t *combination_vector);
int mmpm_compile_eval_combinations(mmpm_handler_t *handler,
                                   const uint32_t *aliases,
                                   uint32_t alias_count,
                                   uint32_t *comb_ids,
                                   uint32_t *comb_count,
                                   uint32_t comb_capacity);
int mmpm_compile_eval_stream_matches(mmpm_handler_t *handler,
                                     const uint32_t *aliases,
                                     uint32_t alias_count,
                                     uint8_t *logical_vector,
                                     uint32_t logical_count,
                                     uint8_t *combination_vector,
                                     uint32_t combination_count,
                                     uint32_t *ids,
                                     uint32_t *id_count,
                                     uint32_t id_capacity);
int mmpm_expr_scan_stream(mmpm_handler_t *handler,
                          ACAutomaton *automaton,
                          uint32_t *node_index,
                          uint8_t *logical_vector,
                          uint32_t logical_count,
                          uint8_t *combination_vector,
                          uint32_t combination_count,
                          uint64_t *offset,
                          const uint8_t *data,
                          uint32_t len,
                          uint32_t *ids,
                          uint32_t *id_count,
                          uint32_t id_capacity);

#endif
