/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "ms.h"
#include "ms_allocator.h"
#include "../ms_database_internal.h"
#include "ms_expr_internal.h"
#include "ms_logical.h"

int ms_compile_alias_capture_reserve(ms_compile_alias_capture_t *capture)
{
    uint32_t *aliases = NULL;
    uint64_t *offsets = NULL;
    uint32_t new_capacity;

    if (capture->count < capture->capacity) {
        return MS_SUCCESS;
    }

    if (capture->capacity > UINT32_MAX / 2u) {
        return MS_MALLOC_FAIL;
    }
    new_capacity = capture->capacity ? capture->capacity * 2u : 4u;
    aliases = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
    offsets = (uint64_t *)ms_calloc(new_capacity, sizeof(uint64_t));
    if (aliases == NULL || offsets == NULL) {
        if (aliases != NULL) {
            ms_free(aliases);
        }
        if (offsets != NULL) {
            ms_free(offsets);
        }
        return MS_MALLOC_FAIL;
    }

    if (capture->aliases != NULL) {
        (void)memcpy(aliases, capture->aliases, capture->count * sizeof(uint32_t));
        ms_free(capture->aliases);
    }
    if (capture->offsets != NULL) {
        (void)memcpy(offsets, capture->offsets, capture->count * sizeof(uint64_t));
        ms_free(capture->offsets);
    }

    capture->aliases = aliases;
    capture->offsets = offsets;
    capture->capacity = new_capacity;
    return MS_SUCCESS;
}

int ms_compile_capture_alias(uint32_t alias, uint64_t end_offset, void *ctx)
{
    ms_compile_alias_capture_t *capture = (ms_compile_alias_capture_t *)ctx;
    int ret;

    if (capture == NULL) {
        return MS_INVALID_PARA;
    }

    ret = ms_compile_alias_capture_reserve(capture);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    capture->aliases[capture->count] = alias;
    capture->offsets[capture->count] = end_offset;
    capture->count++;
    return MS_SUCCESS;
}

int ms_compile_capture_alias_unique(uint32_t alias, uint64_t end_offset, void *ctx)
{
    ms_compile_alias_capture_t *capture = (ms_compile_alias_capture_t *)ctx;
    uint32_t i;
    int ret;

    if (capture == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < capture->count; i++) {
        if (capture->aliases[i] == alias) {
            capture->offsets[i] = end_offset;
            return MS_SUCCESS;
        }
    }

    ret = ms_compile_alias_capture_reserve(capture);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    capture->aliases[capture->count] = alias;
    capture->offsets[capture->count] = end_offset;
    capture->count++;
    return MS_SUCCESS;
}

int ms_compile_mark_aliases(const ms_compile_context_t *ctx,
                              const uint32_t *aliases,
                              uint32_t alias_count,
                              uint8_t *logical_vector,
                              uint32_t logical_count)
{
    uint32_t i;
    int ret;

    if (ctx == NULL || logical_vector == NULL ||
        (alias_count != 0 && aliases == NULL)) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < alias_count; i++) {
        uint32_t lkey;

        ret = ms_compile_context_alias_to_lkey(ctx, aliases[i], &lkey);
        if (ret == MS_FIND_FAIL) {
            continue;
        }
        if (ret != MS_SUCCESS) {
            return ret;
        }

        ret = ms_logical_set(logical_vector, logical_count, lkey, 1);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    return MS_SUCCESS;
}

static int ms_compile_state_init_with_allocator(const ms_database_t *db,
                                                  uint8_t **logical_vector,
                                                  uint32_t *logical_count,
                                                  uint8_t **combination_vector,
                                                  uint32_t *combination_count,
                                                  uint8_t **exhaustion_vector,
                                                  uint32_t *exhaustion_count,
                                                  ms_calloc_t calloc_func,
                                                  ms_free_t free_func)
{
    ms_compile_context_t *ctx;
    uint32_t logical_bytes = 0;
    uint32_t combination_bytes = 0;
    uint32_t exhaustion_bytes = 0;

    if (db == NULL || logical_vector == NULL || logical_count == NULL ||
        combination_vector == NULL || combination_count == NULL ||
        exhaustion_vector == NULL || exhaustion_count == NULL) {
        return MS_INVALID_PARA;
    }

    *logical_vector = NULL;
    *logical_count = 0;
    *combination_vector = NULL;
    *combination_count = 0;
    *exhaustion_vector = NULL;
    *exhaustion_count = 0;

    ctx = (ms_compile_context_t *)db->ms_compile_ctx;
    if (ctx == NULL) {
        return MS_SUCCESS;
    }

    *logical_count = ctx->logical_count;
    *combination_count = ctx->comb_count;
    *exhaustion_count = ctx->ekey_count;
    logical_bytes = ms_compile_logical_byte_count(ctx->logical_count);
    combination_bytes = ms_compile_logical_byte_count(ctx->comb_count);
    exhaustion_bytes = ms_compile_logical_byte_count(ctx->ekey_count);

    if (logical_bytes != 0) {
        *logical_vector = (uint8_t *)calloc_func(logical_bytes, sizeof(uint8_t));
        if (*logical_vector == NULL) {
            return MS_MALLOC_FAIL;
        }
    }

    if (combination_bytes != 0) {
        *combination_vector = (uint8_t *)calloc_func(combination_bytes, sizeof(uint8_t));
        if (*combination_vector == NULL) {
            if (*logical_vector != NULL) {
                free_func(*logical_vector);
                *logical_vector = NULL;
            }
            return MS_MALLOC_FAIL;
        }
    }

    if (exhaustion_bytes != 0) {
        *exhaustion_vector = (uint8_t *)calloc_func(exhaustion_bytes, sizeof(uint8_t));
        if (*exhaustion_vector == NULL) {
            if (*logical_vector != NULL) {
                free_func(*logical_vector);
                *logical_vector = NULL;
            }
            if (*combination_vector != NULL) {
                free_func(*combination_vector);
                *combination_vector = NULL;
            }
            return MS_MALLOC_FAIL;
        }
    }

    return MS_SUCCESS;
}

static void ms_compile_state_destroy_with_allocator(uint8_t *logical_vector,
                                                      uint8_t *combination_vector,
                                                      uint8_t *exhaustion_vector,
                                                      ms_free_t free_func)
{
    if (logical_vector != NULL) {
        free_func(logical_vector);
    }
    if (combination_vector != NULL) {
        free_func(combination_vector);
    }
    if (exhaustion_vector != NULL) {
        free_func(exhaustion_vector);
    }
}

int ms_compile_stream_state_init(const ms_database_t *db,
                                   uint8_t **logical_vector,
                                   uint32_t *logical_count,
                                   uint8_t **combination_vector,
                                   uint32_t *combination_count,
                                   uint8_t **exhaustion_vector,
                                   uint32_t *exhaustion_count)
{
    return ms_compile_state_init_with_allocator(db,
                                                  logical_vector,
                                                  logical_count,
                                                  combination_vector,
                                                  combination_count,
                                                  exhaustion_vector,
                                                  exhaustion_count,
                                                  ms_stream_calloc,
                                                  ms_stream_free);
}

void ms_compile_stream_state_destroy(uint8_t *logical_vector,
                                       uint8_t *combination_vector,
                                       uint8_t *exhaustion_vector)
{
    ms_compile_state_destroy_with_allocator(logical_vector,
                                              combination_vector,
                                              exhaustion_vector,
                                              ms_stream_free);
}

int ms_compile_scratch_state_init(const ms_database_t *db,
                                    uint8_t **logical_vector,
                                    uint32_t *logical_count,
                                    uint8_t **combination_vector,
                                    uint32_t *combination_count,
                                    uint8_t **exhaustion_vector,
                                    uint32_t *exhaustion_count)
{
    return ms_compile_state_init_with_allocator(db,
                                                  logical_vector,
                                                  logical_count,
                                                  combination_vector,
                                                  combination_count,
                                                  exhaustion_vector,
                                                  exhaustion_count,
                                                  ms_scratch_calloc,
                                                  ms_scratch_free);
}

void ms_compile_scratch_state_destroy(uint8_t *logical_vector,
                                        uint8_t *combination_vector,
                                        uint8_t *exhaustion_vector)
{
    ms_compile_state_destroy_with_allocator(logical_vector,
                                              combination_vector,
                                              exhaustion_vector,
                                              ms_scratch_free);
}

int ms_compile_eval_database_combinations(const ms_database_t *db,
                                            const uint32_t *aliases,
                                            uint32_t alias_count,
                                            uint32_t *comb_ids,
                                            uint32_t *comb_count,
                                            uint32_t comb_capacity)
{
    ms_compile_context_t *ctx = NULL;
    uint8_t *logical_vector = NULL;
    uint8_t *exhaustion_vector = NULL;
    uint32_t *active_ckeys = NULL;
    uint32_t active_ckey_count = 0;
    ms_compile_result_t result = {0};
    ms_compile_index_list_t new_exhaustions = {0};
    int ret = MS_SUCCESS;

    if (db == NULL || comb_count == NULL ||
        (alias_count != 0 && aliases == NULL) ||
        (comb_capacity != 0 && comb_ids == NULL)) {
        return MS_INVALID_PARA;
    }

    *comb_count = 0;
    ctx = (ms_compile_context_t *)db->ms_compile_ctx;
    if (ctx == NULL || ctx->comb_count == 0) {
        return MS_SUCCESS;
    }

    if (ctx->logical_count == 0) {
        return MS_INVALID_PARA;
    }

    logical_vector = (uint8_t *)ms_calloc(ms_compile_logical_byte_count(ctx->logical_count), sizeof(uint8_t));
    if (logical_vector == NULL) {
        return MS_MALLOC_FAIL;
    }
    if (ctx->ekey_count != 0) {
        exhaustion_vector = (uint8_t *)ms_calloc(ms_compile_logical_byte_count(ctx->ekey_count),
                                                   sizeof(uint8_t));
        if (exhaustion_vector == NULL) {
            ret = MS_MALLOC_FAIL;
            goto end;
        }
    }

    ret = ms_compile_mark_aliases(ctx, aliases, alias_count, logical_vector, ctx->logical_count);
    if (ret != MS_SUCCESS) {
        goto end;
    }

    ret = ms_compile_context_get_ckeys_for_aliases(ctx, aliases, alias_count,
                                                     &active_ckeys,
                                                     &active_ckey_count);
    if (ret != MS_SUCCESS) {
        goto end;
    }

    ret = ms_compile_eval_active_combinations(ctx, active_ckeys,
                                                active_ckey_count,
                                                logical_vector,
                                                ctx->logical_count,
                                                NULL, 0,
                                                exhaustion_vector,
                                                ctx->ekey_count,
                                                UINT64_MAX,
                                                NULL,
                                                &result, NULL,
                                                &new_exhaustions, false);
    if (ret != MS_SUCCESS) {
        goto end;
    }

    ret = ms_compile_eval_final_combinations(ctx,
                                               logical_vector,
                                               ctx->logical_count,
                                               NULL, 0,
                                               exhaustion_vector,
                                               ctx->ekey_count,
                                               UINT64_MAX,
                                               &result, NULL,
                                               &new_exhaustions, false);
    if (ret != MS_SUCCESS) {
        goto end;
    }

    *comb_count = result.count;
    if (result.count > comb_capacity) {
        ret = MS_INVALID_PARA;
    } else if (result.count != 0 && comb_ids != NULL) {
        (void)memcpy(comb_ids, result.ids, result.count * sizeof(uint32_t));
    }

end:
    if (active_ckeys != NULL) {
        ms_free(active_ckeys);
    }
    ms_compile_result_destroy(&result);
    ms_compile_index_list_destroy(&new_exhaustions);
    if (logical_vector != NULL) {
        ms_free(logical_vector);
    }
    if (exhaustion_vector != NULL) {
        ms_free(exhaustion_vector);
    }
    return ret;
}

__attribute__((visibility("default")))
int ms_compile_eval_combinations(ms_handler_t *handler,
                                   const uint32_t *aliases,
                                   uint32_t alias_count,
                                   uint32_t *comb_ids,
                                   uint32_t *comb_count,
                                   uint32_t comb_capacity)
{
    ms_database_t db_view = {0};

    if (handler == NULL) {
        return MS_INVALID_PARA;
    }

    ms_database_from_handler(&db_view, handler);
    return ms_compile_eval_database_combinations(&db_view,
                                                   aliases,
                                                   alias_count,
                                                   comb_ids,
                                                   comb_count,
                                                   comb_capacity);
}

static int ms_compile_validate_runtime_workspace(
    const ms_compile_context_t *ctx,
    bool require_final_state,
    const ms_compile_runtime_workspace_t *workspace)
{
    uint32_t result_capacity;

    if (ctx == NULL || workspace == NULL) {
        return MS_INVALID_PARA;
    }

    if (ctx->expr_count > UINT32_MAX - ctx->comb_count ||
        ctx->logical_count > UINT32_MAX - ctx->comb_count) {
        return MS_INVALID_PARA;
    }
    result_capacity = ctx->expr_count + ctx->comb_count;
    if ((require_final_state && ctx->comb_count != 0 &&
         ctx->logical_count != 0 &&
         workspace->final_logical_vector == NULL) ||
        (ctx->comb_count != 0 &&
         (workspace->active_ckey_vector == NULL ||
          workspace->active_ckeys == NULL ||
          workspace->active_ckey_capacity < ctx->comb_count ||
          workspace->new_combination_indexes == NULL ||
          workspace->new_combination_capacity < ctx->comb_count)) ||
        (result_capacity != 0 &&
         (workspace->result_ids == NULL ||
          workspace->result_capacity < result_capacity)) ||
        (ctx->logical_count + ctx->comb_count != 0 &&
         (workspace->result_seen_vector == NULL ||
          workspace->result_seen_count < ctx->logical_count + ctx->comb_count)) ||
        (ctx->ekey_count != 0 &&
         (workspace->new_exhaustion_indexes == NULL ||
          workspace->new_exhaustion_capacity < ctx->ekey_count))) {
        return MS_INVALID_PARA;
    }

    return MS_SUCCESS;
}

static int ms_compile_eval_stream_matches_impl(
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
    ms_compile_runtime_workspace_t *workspace)
{
    ms_compile_context_t *ctx;
    ms_compile_result_t result = {0};
    ms_compile_index_list_t new_combinations = {0};
    ms_compile_index_list_t new_exhaustions = {0};
    uint8_t *final_logical_vector = NULL;
    uint32_t *active_ckeys = NULL;
    uint32_t active_ckey_count = 0;
    uint32_t i;
    int ret = MS_SUCCESS;

    if (db == NULL || id_count == NULL ||
        (alias_count != 0 && (aliases == NULL || offsets == NULL)) ||
        (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }

    *id_count = 0;
    ctx = (ms_compile_context_t *)db->ms_compile_ctx;
    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }

    if ((ctx->logical_count != 0 && (logical_vector == NULL || logical_count != ctx->logical_count)) ||
        (ctx->comb_count != 0 && (combination_vector == NULL || combination_count != ctx->comb_count)) ||
        (ctx->ekey_count != 0 && (exhaustion_vector == NULL || exhaustion_count != ctx->ekey_count))) {
        return MS_INVALID_PARA;
    }

    if (workspace != NULL) {
        ret = ms_compile_validate_runtime_workspace(ctx, require_final_state, workspace);
        if (ret != MS_SUCCESS) {
            return ret;
        }
        result.ids = workspace->result_ids;
        result.capacity = workspace->result_capacity;
        result.seen_vector = workspace->result_seen_vector;
        result.seen_count = workspace->result_seen_count;
        /* Scratch clears only keys touched by the previous scan. */
        new_combinations.indexes = workspace->new_combination_indexes;
        new_combinations.capacity = workspace->new_combination_capacity;
        new_exhaustions.indexes = workspace->new_exhaustion_indexes;
        new_exhaustions.capacity = workspace->new_exhaustion_capacity;
    }

    if (require_final_state && ctx->comb_count != 0 &&
        ctx->logical_count != 0) {
        if (workspace != NULL) {
            final_logical_vector = workspace->final_logical_vector;
        } else {
            final_logical_vector = (uint8_t *)ms_calloc(
                ms_compile_logical_byte_count(ctx->logical_count), sizeof(uint8_t));
            if (final_logical_vector == NULL) {
                ret = MS_MALLOC_FAIL;
                goto end;
            }
        }
        ret = ms_compile_mark_aliases(ctx, aliases, alias_count,
                                        final_logical_vector, ctx->logical_count);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

    for (i = 0; i < alias_count; i++) {
        const ms_compile_expr_t *expr =
            ms_compile_context_find_expr_for_alias(ctx, aliases[i]);
        uint64_t event_offset = scan_start_offset + offsets[i];

        if (expr == NULL) {
            continue;
        }
        if ((expr->flags & MS_FLAG_QUIET) == 0) {
            ret = ms_compile_result_add_key(&result, expr->id, expr->lkey);
            if (ret != MS_SUCCESS) {
                goto end;
            }
        }
        if (ctx->comb_count == 0) {
            continue;
        }

        ret = ms_compile_mark_aliases(ctx, &aliases[i], 1, logical_vector, logical_count);
        if (ret != MS_SUCCESS) {
            goto end;
        }

        if (workspace != NULL) {
            active_ckeys = workspace->active_ckeys;
            ret = ms_compile_context_collect_ckeys(
                ctx, &aliases[i], 1,
                workspace->active_ckey_vector, ctx->comb_count,
                active_ckeys, workspace->active_ckey_capacity,
                &active_ckey_count);
        } else {
            ret = ms_compile_context_get_ckeys_for_aliases(ctx, &aliases[i], 1,
                                                             &active_ckeys,
                                                             &active_ckey_count);
        }
        if (ret != MS_SUCCESS) {
            goto end;
        }

        ret = ms_compile_eval_active_combinations(ctx, active_ckeys,
                                                    active_ckey_count,
                                                    logical_vector,
                                                    logical_count,
                                                    combination_vector,
                                                    combination_count,
                                                    exhaustion_vector,
                                                    exhaustion_count,
                                                    event_offset,
                                                    final_logical_vector,
                                                    &result,
                                                    &new_combinations,
                                                    &new_exhaustions,
                                                    true);
        if (ret != MS_SUCCESS) {
            goto end;
        }
        if (workspace == NULL && active_ckeys != NULL) {
            ms_free(active_ckeys);
            active_ckeys = NULL;
        }
        active_ckey_count = 0;
    }

    if (final_flush && ctx->comb_count != 0) {
        ret = ms_compile_eval_final_combinations(ctx,
                                                   logical_vector,
                                                   logical_count,
                                                   combination_vector,
                                                   combination_count,
                                                   exhaustion_vector,
                                                   exhaustion_count,
                                                   final_flush_offset,
                                                   &result,
                                                   &new_combinations,
                                                   &new_exhaustions,
                                                   true);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

    *id_count = result.count;
    if (result.count > id_capacity) {
        ret = MS_INVALID_PARA;
    } else {
        if (result.count != 0 && ids != NULL) {
            (void)memcpy(ids, result.ids, result.count * sizeof(uint32_t));
        }
        for (i = 0; i < new_combinations.count; i++) {
            ret = ms_compile_bitset_set(combination_vector, combination_count,
                                          new_combinations.indexes[i], 1);
            if (ret != MS_SUCCESS) {
                goto end;
            }
        }
        ret = (result.count == 0) ? MS_FIND_FAIL : MS_SUCCESS;
    }

end:
    if (workspace == NULL && active_ckeys != NULL) {
        ms_free(active_ckeys);
    }
    if (workspace == NULL && final_logical_vector != NULL) {
        ms_free(final_logical_vector);
    }
    if (workspace == NULL) {
        ms_compile_index_list_destroy(&new_combinations);
        ms_compile_index_list_destroy(&new_exhaustions);
        ms_compile_result_destroy(&result);
    }
    return ret;
}

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
                                     uint32_t id_capacity)
{
    return ms_compile_eval_stream_matches_impl(
        db, aliases, offsets, alias_count,
        logical_vector, logical_count,
        combination_vector, combination_count,
        exhaustion_vector, exhaustion_count,
        scan_start_offset, final_flush_offset,
        final_flush, require_final_state,
        ids, id_count, id_capacity, NULL);
}

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
    ms_compile_runtime_workspace_t *workspace)
{
    return ms_compile_eval_stream_matches_impl(
        db, aliases, offsets, alias_count,
        logical_vector, logical_count,
        combination_vector, combination_count,
        exhaustion_vector, exhaustion_count,
        scan_start_offset, final_flush_offset,
        final_flush, require_final_state,
        ids, id_count, id_capacity, workspace);
}
