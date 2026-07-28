/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#ifndef MS_EXPR_INTERNAL_H
#define MS_EXPR_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "ms.h"
#include "ms_handler.h"
#include "ms_ac.h"
#include "ms_logical.h"

typedef struct {
    uint32_t id;
    uint32_t flags;
    uint32_t lkey;
} ms_compile_expr_t;

typedef struct {
    uint32_t id;
    uint32_t flags;
    uint32_t ckey;
    uint32_t ekey;
    uint32_t lkey_start;
    uint32_t lkey_result;
    uint32_t result;
    uint64_t min_offset;
    uint64_t max_offset;
    bool can_match_without_active_lkey;
    ms_logical_tree_t tree;
} ms_compile_comb_t;

typedef struct {
    ms_compile_expr_t *exprs;
    uint32_t expr_count;
    uint32_t expr_capacity;
    ms_compile_comb_t *combs;
    uint32_t comb_count;
    uint32_t comb_capacity;
    uint32_t **lkey_to_ckeys;
    uint32_t *lkey_to_ckey_counts;
    uint32_t *lkey_to_ckey_capacities;
    uint32_t *lkey_to_expr_indexes;
    uint32_t lkey_to_ckey_capacity;
    /* Only combinations that can become true without an active lkey need final evaluation. */
    uint32_t *final_ckeys;
    uint32_t final_ckey_count;
    uint32_t final_ckey_capacity;
    uint32_t logical_count;
    uint32_t ekey_count;
    bool ac_aliases_are_lkeys;
} ms_compile_context_t;

typedef struct {
    uint32_t *aliases;
    uint64_t *offsets;
    uint32_t count;
    uint32_t capacity;
} ms_compile_alias_capture_t;

typedef struct {
    uint32_t *ids;
    uint32_t count;
    uint32_t capacity;
    uint8_t *seen_vector;
    uint32_t seen_count;
} ms_compile_result_t;

typedef struct {
    uint32_t *indexes;
    uint32_t count;
    uint32_t capacity;
} ms_compile_index_list_t;

typedef struct {
    uint8_t *final_logical_vector;
    uint8_t *active_ckey_vector;
    uint32_t *active_ckeys;
    uint32_t active_ckey_capacity;
    uint32_t *result_ids;
    uint32_t result_capacity;
    uint8_t *result_seen_vector;
    uint32_t result_seen_count;
    uint32_t *new_combination_indexes;
    uint32_t new_combination_capacity;
    uint32_t *new_exhaustion_indexes;
    uint32_t new_exhaustion_capacity;
} ms_compile_runtime_workspace_t;

bool ms_compile_is_combination(const uint32_t *flags, uint32_t index);
bool ms_compile_context_has_id(const ms_compile_context_t *ctx, uint32_t id);
bool ms_compile_context_has_comb_id(const ms_compile_context_t *ctx, uint32_t id);
int ms_compile_context_find_lkey(const ms_compile_context_t *ctx,
                                   uint32_t id,
                                   uint32_t *lkey);
const ms_compile_expr_t *ms_compile_context_find_expr(const ms_compile_context_t *ctx,
                                                          uint32_t id);
const ms_compile_expr_t *ms_compile_context_find_expr_by_lkey(
    const ms_compile_context_t *ctx, uint32_t lkey);
const ms_compile_expr_t *ms_compile_context_find_expr_for_alias(
    const ms_compile_context_t *ctx, uint32_t alias);
int ms_compile_context_alias_to_lkey(const ms_compile_context_t *ctx,
                                       uint32_t alias,
                                       uint32_t *lkey);
uint32_t ms_compile_logical_byte_count(uint32_t logical_count);
int ms_compile_context_add_expr(ms_compile_context_t *ctx,
                                  uint32_t id,
                                  uint32_t flags,
                                  uint32_t lkey);
int ms_compile_context_add_comb(ms_compile_context_t *ctx,
                                  uint32_t id,
                                  uint32_t flags,
                                  const ms_expr_ext_t *ext,
                                  ms_logical_tree_t *tree);
int ms_compile_context_remove_comb_id(ms_compile_context_t *ctx, uint32_t id);
int ms_compile_context_remove_id(ms_compile_context_t *ctx, uint32_t id);
int ms_compile_context_clone(const ms_compile_context_t *ctx,
                              ms_compile_context_t **clone);
int ms_compile_append_context(ms_handler_t *handler,
                                ms_compile_context_t *new_ctx);
int ms_compile_append_database_context(ms_database_t *db,
                                         ms_compile_context_t *new_ctx);
int ms_compile_database_multi_native(ms_database_t *db,
                                       const char *const *expressions,
                                       const uint32_t *flags,
                                       const uint32_t *ids,
                                       const ms_expr_ext_t *ext,
                                       uint32_t elements);
int ms_compile_mark_aliases(const ms_compile_context_t *ctx,
                              const uint32_t *aliases,
                              uint32_t alias_count,
                              uint8_t *logical_vector,
                              uint32_t logical_count);
int ms_compile_bitset_get(const uint8_t *vector,
                            uint32_t count,
                            uint32_t key,
                            uint8_t *value);
int ms_compile_bitset_set(uint8_t *vector,
                            uint32_t count,
                            uint32_t key,
                            uint8_t value);
int ms_compile_context_get_ckeys_for_aliases(const ms_compile_context_t *ctx,
                                               const uint32_t *aliases,
                                               uint32_t alias_count,
                                               uint32_t **ckeys,
                                               uint32_t *ckey_count);
int ms_compile_context_collect_ckeys(const ms_compile_context_t *ctx,
                                       const uint32_t *aliases,
                                       uint32_t alias_count,
                                       uint8_t *active_vector,
                                       uint32_t active_count,
                                       uint32_t *ckeys,
                                       uint32_t ckey_capacity,
                                       uint32_t *ckey_count);
int ms_compile_result_add(ms_compile_result_t *result, uint32_t id);
int ms_compile_result_add_key(ms_compile_result_t *result, uint32_t id,
                                uint32_t key);
void ms_compile_result_destroy(ms_compile_result_t *result);
void ms_compile_index_list_destroy(ms_compile_index_list_t *list);
int ms_compile_eval_active_combinations(const ms_compile_context_t *ctx,
                                          const uint32_t *active_ckeys,
                                          uint32_t active_ckey_count,
                                          uint8_t *logical_vector,
                                          uint32_t logical_count,
                                          uint8_t *combination_vector,
                                          uint32_t combination_count,
                                          uint8_t *exhaustion_vector,
                                          uint32_t exhaustion_count,
                                          uint64_t end_offset,
                                          uint8_t *final_logical_vector,
                                          ms_compile_result_t *result,
                                          ms_compile_index_list_t *new_combinations,
                                          ms_compile_index_list_t *new_exhaustions,
                                          bool use_combination_vector);
int ms_compile_eval_final_combinations(const ms_compile_context_t *ctx,
                                         uint8_t *logical_vector,
                                         uint32_t logical_count,
                                         uint8_t *combination_vector,
                                         uint32_t combination_count,
                                         uint8_t *exhaustion_vector,
                                         uint32_t exhaustion_count,
                                         uint64_t end_offset,
                                         ms_compile_result_t *result,
                                         ms_compile_index_list_t *new_combinations,
                                         ms_compile_index_list_t *new_exhaustions,
                                         bool use_combination_vector);
void ms_compile_context_free(ms_compile_context_t *ctx);
void ms_compile_context_destroy_database(ms_database_t *db);
int ms_compile_alias_capture_reserve(ms_compile_alias_capture_t *capture);
int ms_compile_capture_alias(uint32_t alias, uint64_t end_offset, void *ctx);
int ms_compile_capture_alias_unique(uint32_t alias, uint64_t end_offset, void *ctx);

int ms_compile_stream_state_init(const ms_database_t *db,
                                   uint8_t **logical_vector,
                                   uint32_t *logical_count,
                                   uint8_t **combination_vector,
                                   uint32_t *combination_count,
                                   uint8_t **exhaustion_vector,
                                   uint32_t *exhaustion_count);
void ms_compile_stream_state_destroy(uint8_t *logical_vector,
                                       uint8_t *combination_vector,
                                       uint8_t *exhaustion_vector);
int ms_compile_scratch_state_init(const ms_database_t *db,
                                    uint8_t **logical_vector,
                                    uint32_t *logical_count,
                                    uint8_t **combination_vector,
                                    uint32_t *combination_count,
                                    uint8_t **exhaustion_vector,
                                    uint32_t *exhaustion_count);
void ms_compile_scratch_state_destroy(uint8_t *logical_vector,
                                        uint8_t *combination_vector,
                                        uint8_t *exhaustion_vector);
int ms_compile_eval_database_combinations(const ms_database_t *db,
                                            const uint32_t *aliases,
                                            uint32_t alias_count,
                                            uint32_t *comb_ids,
                                            uint32_t *comb_count,
                                            uint32_t comb_capacity);
int ms_compile_eval_combinations(ms_handler_t *handler,
                                   const uint32_t *aliases,
                                   uint32_t alias_count,
                                   uint32_t *comb_ids,
                                   uint32_t *comb_count,
                                   uint32_t comb_capacity);
int ms_compile_eval_stream_matches(const ms_database_t *db,
                                     const uint32_t *aliases,
                                     const uint64_t *offsets,
                                     uint32_t alias_count,
                                     uint8_t *logical_vector,
                                     uint32_t logical_count,
                                     uint8_t *combination_vector,
                                     uint32_t combination_count,
                                     uint8_t *exhaustion_vector,
                                     uint32_t exhaustion_count,
                                     uint64_t scan_start_offset,
                                     uint64_t final_flush_offset,
                                     bool final_flush,
                                     bool require_final_state,
                                     uint32_t *ids,
                                     uint32_t *id_count,
                                     uint32_t id_capacity);
int ms_compile_eval_stream_matches_with_workspace(
    const ms_database_t *db,
    const uint32_t *aliases,
    const uint64_t *offsets,
    uint32_t alias_count,
    uint8_t *logical_vector,
    uint32_t logical_count,
    uint8_t *combination_vector,
    uint32_t combination_count,
    uint8_t *exhaustion_vector,
    uint32_t exhaustion_count,
    uint64_t scan_start_offset,
    uint64_t final_flush_offset,
    bool final_flush,
    bool require_final_state,
    uint32_t *ids,
    uint32_t *id_count,
    uint32_t id_capacity,
    ms_compile_runtime_workspace_t *workspace);
int ms_expr_scan_stream(const ms_database_t *db,
                          ACAutomaton *automaton,
                          uint32_t *node_index,
                          uint8_t *logical_vector,
                          uint32_t logical_count,
                          uint8_t *combination_vector,
                          uint32_t combination_count,
                          uint8_t *exhaustion_vector,
                          uint32_t exhaustion_count,
                          uint64_t *offset,
                          const uint8_t *data,
                          uint32_t len,
                          uint32_t *ids,
                          uint32_t *id_count,
                          uint32_t id_capacity);
int ms_expr_flush_stream(const ms_database_t *db,
                           uint8_t *logical_vector,
                           uint32_t logical_count,
                           uint8_t *combination_vector,
                           uint32_t combination_count,
                           uint8_t *exhaustion_vector,
                           uint32_t exhaustion_count,
                           uint64_t offset,
                           uint32_t *ids,
                           uint32_t *id_count,
                           uint32_t id_capacity);

#endif
