/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdint.h>
#include <string.h>

#include "mmpm.h"
#include "mmpm_allocator.h"
#include "mmpm_expr_internal.h"
#include "mmpm_logical.h"

int mmpm_compile_alias_capture_reserve(mmpm_compile_alias_capture_t *capture)
{
    uint32_t *aliases = NULL;
    uint64_t *offsets = NULL;
    uint32_t new_capacity;

    if (capture->count < capture->capacity) {
        return MMPM_SUCCESS;
    }

    new_capacity = capture->capacity ? capture->capacity * 2 : 4;
    aliases = (uint32_t *)mmpm_calloc(new_capacity, sizeof(uint32_t));
    offsets = (uint64_t *)mmpm_calloc(new_capacity, sizeof(uint64_t));
    if (aliases == NULL || offsets == NULL) {
        if (aliases != NULL) {
            mmpm_free(aliases);
        }
        if (offsets != NULL) {
            mmpm_free(offsets);
        }
        return MMPM_MALLOC_FAIL;
    }

    if (capture->aliases != NULL) {
        (void)memcpy(aliases, capture->aliases, capture->count * sizeof(uint32_t));
        mmpm_free(capture->aliases);
    }
    if (capture->offsets != NULL) {
        (void)memcpy(offsets, capture->offsets, capture->count * sizeof(uint64_t));
        mmpm_free(capture->offsets);
    }

    capture->aliases = aliases;
    capture->offsets = offsets;
    capture->capacity = new_capacity;
    return MMPM_SUCCESS;
}

int mmpm_compile_capture_alias(uint32_t alias, uint64_t end_offset, void *ctx)
{
    mmpm_compile_alias_capture_t *capture = (mmpm_compile_alias_capture_t *)ctx;
    int ret;

    if (capture == NULL) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_compile_alias_capture_reserve(capture);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    capture->aliases[capture->count] = alias;
    capture->offsets[capture->count] = end_offset;
    capture->count++;
    return MMPM_SUCCESS;
}

int mmpm_compile_capture_alias_unique(uint32_t alias, uint64_t end_offset, void *ctx)
{
    mmpm_compile_alias_capture_t *capture = (mmpm_compile_alias_capture_t *)ctx;
    uint32_t i;
    int ret;

    if (capture == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < capture->count; i++) {
        if (capture->aliases[i] == alias) {
            capture->offsets[i] = end_offset;
            return MMPM_SUCCESS;
        }
    }

    ret = mmpm_compile_alias_capture_reserve(capture);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    capture->aliases[capture->count] = alias;
    capture->offsets[capture->count] = end_offset;
    capture->count++;
    return MMPM_SUCCESS;
}

int mmpm_compile_mark_aliases(const mmpm_compile_context_t *ctx,
                              const uint32_t *aliases,
                              uint32_t alias_count,
                              uint8_t *logical_vector,
                              uint32_t logical_count)
{
    uint32_t i;
    int ret;

    if (ctx == NULL || logical_vector == NULL ||
        (alias_count != 0 && aliases == NULL)) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < alias_count; i++) {
        uint32_t lkey;

        ret = mmpm_compile_context_find_lkey(ctx, aliases[i], &lkey);
        if (ret == MMPM_FIND_FAIL) {
            continue;
        }
        if (ret != MMPM_SUCCESS) {
            return ret;
        }

        ret = mmpm_logical_set(logical_vector, logical_count, lkey, 1);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_state_init_with_allocator(mmpm_handler_t *handler,
                                                  uint8_t **logical_vector,
                                                  uint32_t *logical_count,
                                                  uint8_t **combination_vector,
                                                  uint32_t *combination_count,
                                                  uint8_t **exhaustion_vector,
                                                  uint32_t *exhaustion_count,
                                                  mmpm_calloc_t calloc_func,
                                                  mmpm_free_t free_func)
{
    mmpm_compile_context_t *ctx;
    uint32_t logical_bytes = 0;
    uint32_t combination_bytes = 0;
    uint32_t exhaustion_bytes = 0;

    if (handler == NULL || logical_vector == NULL || logical_count == NULL ||
        combination_vector == NULL || combination_count == NULL ||
        exhaustion_vector == NULL || exhaustion_count == NULL) {
        return MMPM_INVALID_PARA;
    }

    *logical_vector = NULL;
    *logical_count = 0;
    *combination_vector = NULL;
    *combination_count = 0;
    *exhaustion_vector = NULL;
    *exhaustion_count = 0;

    ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    if (ctx == NULL) {
        return MMPM_SUCCESS;
    }

    *logical_count = ctx->logical_count;
    *combination_count = ctx->comb_count;
    *exhaustion_count = ctx->ekey_count;
    logical_bytes = mmpm_compile_logical_byte_count(ctx->logical_count);
    combination_bytes = mmpm_compile_logical_byte_count(ctx->comb_count);
    exhaustion_bytes = mmpm_compile_logical_byte_count(ctx->ekey_count);

    if (logical_bytes != 0) {
        *logical_vector = (uint8_t *)calloc_func(logical_bytes, sizeof(uint8_t));
        if (*logical_vector == NULL) {
            return MMPM_MALLOC_FAIL;
        }
    }

    if (combination_bytes != 0) {
        *combination_vector = (uint8_t *)calloc_func(combination_bytes, sizeof(uint8_t));
        if (*combination_vector == NULL) {
            if (*logical_vector != NULL) {
                free_func(*logical_vector);
                *logical_vector = NULL;
            }
            return MMPM_MALLOC_FAIL;
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
            return MMPM_MALLOC_FAIL;
        }
    }

    return MMPM_SUCCESS;
}

static void mmpm_compile_state_destroy_with_allocator(uint8_t *logical_vector,
                                                      uint8_t *combination_vector,
                                                      uint8_t *exhaustion_vector,
                                                      mmpm_free_t free_func)
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

int mmpm_compile_stream_state_init(mmpm_handler_t *handler,
                                   uint8_t **logical_vector,
                                   uint32_t *logical_count,
                                   uint8_t **combination_vector,
                                   uint32_t *combination_count,
                                   uint8_t **exhaustion_vector,
                                   uint32_t *exhaustion_count)
{
    return mmpm_compile_state_init_with_allocator(handler,
                                                  logical_vector,
                                                  logical_count,
                                                  combination_vector,
                                                  combination_count,
                                                  exhaustion_vector,
                                                  exhaustion_count,
                                                  mmpm_stream_calloc,
                                                  mmpm_stream_free);
}

void mmpm_compile_stream_state_destroy(uint8_t *logical_vector,
                                       uint8_t *combination_vector,
                                       uint8_t *exhaustion_vector)
{
    mmpm_compile_state_destroy_with_allocator(logical_vector,
                                              combination_vector,
                                              exhaustion_vector,
                                              mmpm_stream_free);
}

int mmpm_compile_scratch_state_init(mmpm_handler_t *handler,
                                    uint8_t **logical_vector,
                                    uint32_t *logical_count,
                                    uint8_t **combination_vector,
                                    uint32_t *combination_count,
                                    uint8_t **exhaustion_vector,
                                    uint32_t *exhaustion_count)
{
    return mmpm_compile_state_init_with_allocator(handler,
                                                  logical_vector,
                                                  logical_count,
                                                  combination_vector,
                                                  combination_count,
                                                  exhaustion_vector,
                                                  exhaustion_count,
                                                  mmpm_scratch_calloc,
                                                  mmpm_scratch_free);
}

void mmpm_compile_scratch_state_destroy(uint8_t *logical_vector,
                                        uint8_t *combination_vector,
                                        uint8_t *exhaustion_vector)
{
    mmpm_compile_state_destroy_with_allocator(logical_vector,
                                              combination_vector,
                                              exhaustion_vector,
                                              mmpm_scratch_free);
}

__attribute__((visibility("default")))
int mmpm_compile_eval_combinations(mmpm_handler_t *handler,
                                   const uint32_t *aliases,
                                   uint32_t alias_count,
                                   uint32_t *comb_ids,
                                   uint32_t *comb_count,
                                   uint32_t comb_capacity)
{
    mmpm_compile_context_t *ctx = NULL;
    uint8_t *logical_vector = NULL;
    uint8_t *exhaustion_vector = NULL;
    uint32_t *active_ckeys = NULL;
    uint32_t active_ckey_count = 0;
    mmpm_compile_result_t result = {0};
    mmpm_compile_index_list_t new_exhaustions = {0};
    int ret = MMPM_SUCCESS;

    if (handler == NULL || comb_count == NULL ||
        (alias_count != 0 && aliases == NULL) ||
        (comb_capacity != 0 && comb_ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    *comb_count = 0;
    ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    if (ctx == NULL || ctx->comb_count == 0) {
        return MMPM_SUCCESS;
    }

    if (ctx->logical_count == 0) {
        return MMPM_INVALID_PARA;
    }

    logical_vector = (uint8_t *)mmpm_calloc(mmpm_compile_logical_byte_count(ctx->logical_count), sizeof(uint8_t));
    if (logical_vector == NULL) {
        return MMPM_MALLOC_FAIL;
    }
    if (ctx->ekey_count != 0) {
        exhaustion_vector = (uint8_t *)mmpm_calloc(mmpm_compile_logical_byte_count(ctx->ekey_count),
                                                   sizeof(uint8_t));
        if (exhaustion_vector == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
    }

    ret = mmpm_compile_mark_aliases(ctx, aliases, alias_count, logical_vector, ctx->logical_count);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    ret = mmpm_compile_context_get_ckeys_for_aliases(ctx, aliases, alias_count,
                                                     &active_ckeys,
                                                     &active_ckey_count);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    ret = mmpm_compile_eval_active_combinations(ctx, active_ckeys,
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
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    ret = mmpm_compile_eval_final_combinations(ctx,
                                               logical_vector,
                                               ctx->logical_count,
                                               NULL, 0,
                                               exhaustion_vector,
                                               ctx->ekey_count,
                                               UINT64_MAX,
                                               &result, NULL,
                                               &new_exhaustions, false);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    *comb_count = result.count;
    if (result.count > comb_capacity) {
        ret = MMPM_INVALID_PARA;
    } else if (result.count != 0 && comb_ids != NULL) {
        (void)memcpy(comb_ids, result.ids, result.count * sizeof(uint32_t));
    }

end:
    if (active_ckeys != NULL) {
        mmpm_free(active_ckeys);
    }
    mmpm_compile_result_destroy(&result);
    mmpm_compile_index_list_destroy(&new_exhaustions);
    if (logical_vector != NULL) {
        mmpm_free(logical_vector);
    }
    if (exhaustion_vector != NULL) {
        mmpm_free(exhaustion_vector);
    }
    return ret;
}

static int mmpm_compile_validate_runtime_workspace(
    const mmpm_compile_context_t *ctx,
    bool require_final_state,
    const mmpm_compile_runtime_workspace_t *workspace)
{
    uint32_t result_capacity;

    if (ctx == NULL || workspace == NULL) {
        return MMPM_INVALID_PARA;
    }

    result_capacity = ctx->expr_count + ctx->comb_count;
    if ((require_final_state && ctx->logical_count != 0 &&
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
        (ctx->ekey_count != 0 &&
         (workspace->new_exhaustion_indexes == NULL ||
          workspace->new_exhaustion_capacity < ctx->ekey_count))) {
        return MMPM_INVALID_PARA;
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_eval_stream_matches_impl(
    mmpm_handler_t *handler,
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
    mmpm_compile_runtime_workspace_t *workspace)
{
    mmpm_compile_context_t *ctx;
    mmpm_compile_result_t result = {0};
    mmpm_compile_index_list_t new_combinations = {0};
    mmpm_compile_index_list_t new_exhaustions = {0};
    uint8_t *final_logical_vector = NULL;
    uint32_t *active_ckeys = NULL;
    uint32_t active_ckey_count = 0;
    uint32_t i;
    int ret = MMPM_SUCCESS;

    if (handler == NULL || id_count == NULL ||
        (alias_count != 0 && (aliases == NULL || offsets == NULL)) ||
        (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    *id_count = 0;
    ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    if ((ctx->logical_count != 0 && (logical_vector == NULL || logical_count != ctx->logical_count)) ||
        (ctx->comb_count != 0 && (combination_vector == NULL || combination_count != ctx->comb_count)) ||
        (ctx->ekey_count != 0 && (exhaustion_vector == NULL || exhaustion_count != ctx->ekey_count))) {
        return MMPM_INVALID_PARA;
    }

    if (workspace != NULL) {
        ret = mmpm_compile_validate_runtime_workspace(ctx, require_final_state, workspace);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
        result.ids = workspace->result_ids;
        result.capacity = workspace->result_capacity;
        new_combinations.indexes = workspace->new_combination_indexes;
        new_combinations.capacity = workspace->new_combination_capacity;
        new_exhaustions.indexes = workspace->new_exhaustion_indexes;
        new_exhaustions.capacity = workspace->new_exhaustion_capacity;
    }

    if (require_final_state && ctx->logical_count != 0) {
        if (workspace != NULL) {
            final_logical_vector = workspace->final_logical_vector;
            (void)memset(final_logical_vector, 0,
                         mmpm_compile_logical_byte_count(ctx->logical_count));
        } else {
            final_logical_vector = (uint8_t *)mmpm_calloc(
                mmpm_compile_logical_byte_count(ctx->logical_count), sizeof(uint8_t));
            if (final_logical_vector == NULL) {
                ret = MMPM_MALLOC_FAIL;
                goto end;
            }
        }
        ret = mmpm_compile_mark_aliases(ctx, aliases, alias_count,
                                        final_logical_vector, ctx->logical_count);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

    for (i = 0; i < alias_count; i++) {
        const mmpm_compile_expr_t *expr = mmpm_compile_context_find_expr(ctx, aliases[i]);
        uint64_t event_offset = scan_start_offset + offsets[i];

        if (expr == NULL) {
            continue;
        }
        if ((expr->flags & MMPM_FLAG_QUIET) == 0) {
            ret = mmpm_compile_result_add(&result, expr->id);
            if (ret != MMPM_SUCCESS) {
                goto end;
            }
        }

        ret = mmpm_compile_mark_aliases(ctx, &aliases[i], 1, logical_vector, logical_count);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }

        if (workspace != NULL) {
            active_ckeys = workspace->active_ckeys;
            ret = mmpm_compile_context_collect_ckeys(
                ctx, &aliases[i], 1,
                workspace->active_ckey_vector, ctx->comb_count,
                active_ckeys, workspace->active_ckey_capacity,
                &active_ckey_count);
        } else {
            ret = mmpm_compile_context_get_ckeys_for_aliases(ctx, &aliases[i], 1,
                                                             &active_ckeys,
                                                             &active_ckey_count);
        }
        if (ret != MMPM_SUCCESS) {
            goto end;
        }

        ret = mmpm_compile_eval_active_combinations(ctx, active_ckeys,
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
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
        if (workspace == NULL && active_ckeys != NULL) {
            mmpm_free(active_ckeys);
            active_ckeys = NULL;
        }
        active_ckey_count = 0;
    }

    if (final_flush) {
        ret = mmpm_compile_eval_final_combinations(ctx,
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
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

    *id_count = result.count;
    if (result.count > id_capacity) {
        ret = MMPM_INVALID_PARA;
    } else {
        if (result.count != 0 && ids != NULL) {
            (void)memcpy(ids, result.ids, result.count * sizeof(uint32_t));
        }
        for (i = 0; i < new_combinations.count; i++) {
            ret = mmpm_compile_bitset_set(combination_vector, combination_count,
                                          new_combinations.indexes[i], 1);
            if (ret != MMPM_SUCCESS) {
                goto end;
            }
        }
        ret = (result.count == 0) ? MMPM_FIND_FAIL : MMPM_SUCCESS;
    }

end:
    if (workspace == NULL && active_ckeys != NULL) {
        mmpm_free(active_ckeys);
    }
    if (workspace == NULL && final_logical_vector != NULL) {
        mmpm_free(final_logical_vector);
    }
    if (workspace == NULL) {
        mmpm_compile_index_list_destroy(&new_combinations);
        mmpm_compile_index_list_destroy(&new_exhaustions);
        mmpm_compile_result_destroy(&result);
    }
    return ret;
}

int mmpm_compile_eval_stream_matches(mmpm_handler_t *handler,
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
    return mmpm_compile_eval_stream_matches_impl(
        handler, aliases, offsets, alias_count,
        logical_vector, logical_count,
        combination_vector, combination_count,
        exhaustion_vector, exhaustion_count,
        scan_start_offset, final_flush_offset,
        final_flush, require_final_state,
        ids, id_count, id_capacity, NULL);
}

int mmpm_compile_eval_stream_matches_with_workspace(
    mmpm_handler_t *handler,
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
    mmpm_compile_runtime_workspace_t *workspace)
{
    return mmpm_compile_eval_stream_matches_impl(
        handler, aliases, offsets, alias_count,
        logical_vector, logical_count,
        combination_vector, combination_count,
        exhaustion_vector, exhaustion_count,
        scan_start_offset, final_flush_offset,
        final_flush, require_final_state,
        ids, id_count, id_capacity, workspace);
}
