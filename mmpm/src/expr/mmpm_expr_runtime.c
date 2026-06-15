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
    uint32_t new_capacity;

    if (capture->count < capture->capacity) {
        return MMPM_SUCCESS;
    }

    new_capacity = capture->capacity ? capture->capacity * 2 : 4;
    aliases = (uint32_t *)mmpm_calloc(new_capacity, sizeof(uint32_t));
    if (aliases == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    if (capture->aliases != NULL) {
        (void)memcpy(aliases, capture->aliases, capture->count * sizeof(uint32_t));
        mmpm_free(capture->aliases);
    }

    capture->aliases = aliases;
    capture->capacity = new_capacity;
    return MMPM_SUCCESS;
}

int mmpm_compile_capture_alias(uint32_t alias, void *ctx)
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
    capture->count++;
    return MMPM_SUCCESS;
}

int mmpm_compile_capture_alias_unique(uint32_t alias, void *ctx)
{
    mmpm_compile_alias_capture_t *capture = (mmpm_compile_alias_capture_t *)ctx;
    uint32_t i;
    int ret;

    if (capture == NULL) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < capture->count; i++) {
        if (capture->aliases[i] == alias) {
            return MMPM_SUCCESS;
        }
    }

    ret = mmpm_compile_alias_capture_reserve(capture);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    capture->aliases[capture->count] = alias;
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

int mmpm_compile_stream_state_init(mmpm_handler_t *handler,
                                   uint8_t **logical_vector,
                                   uint32_t *logical_count,
                                   uint8_t **combination_vector,
                                   uint32_t *combination_count,
                                   uint8_t **exhaustion_vector,
                                   uint32_t *exhaustion_count)
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
        *logical_vector = (uint8_t *)mmpm_calloc(logical_bytes, sizeof(uint8_t));
        if (*logical_vector == NULL) {
            return MMPM_MALLOC_FAIL;
        }
    }

    if (combination_bytes != 0) {
        *combination_vector = (uint8_t *)mmpm_calloc(combination_bytes, sizeof(uint8_t));
        if (*combination_vector == NULL) {
            if (*logical_vector != NULL) {
                mmpm_free(*logical_vector);
                *logical_vector = NULL;
            }
            return MMPM_MALLOC_FAIL;
        }
    }

    if (exhaustion_bytes != 0) {
        *exhaustion_vector = (uint8_t *)mmpm_calloc(exhaustion_bytes, sizeof(uint8_t));
        if (*exhaustion_vector == NULL) {
            if (*logical_vector != NULL) {
                mmpm_free(*logical_vector);
                *logical_vector = NULL;
            }
            if (*combination_vector != NULL) {
                mmpm_free(*combination_vector);
                *combination_vector = NULL;
            }
            return MMPM_MALLOC_FAIL;
        }
    }

    return MMPM_SUCCESS;
}

void mmpm_compile_stream_state_destroy(uint8_t *logical_vector,
                                       uint8_t *combination_vector,
                                       uint8_t *exhaustion_vector)
{
    if (logical_vector != NULL) {
        mmpm_free(logical_vector);
    }
    if (combination_vector != NULL) {
        mmpm_free(combination_vector);
    }
    if (exhaustion_vector != NULL) {
        mmpm_free(exhaustion_vector);
    }
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
                                                &result, NULL, false);
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
                                               &result, NULL, false);
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
    if (logical_vector != NULL) {
        mmpm_free(logical_vector);
    }
    if (exhaustion_vector != NULL) {
        mmpm_free(exhaustion_vector);
    }
    return ret;
}

int mmpm_compile_eval_stream_matches(mmpm_handler_t *handler,
                                     const uint32_t *aliases,
                                     uint32_t alias_count,
                                     uint8_t *logical_vector,
                                     uint32_t logical_count,
                                     uint8_t *combination_vector,
                                     uint32_t combination_count,
                                     uint8_t *exhaustion_vector,
                                     uint32_t exhaustion_count,
                                     uint64_t end_offset,
                                     bool final_flush,
                                     uint32_t *ids,
                                     uint32_t *id_count,
                                     uint32_t id_capacity)
{
    mmpm_compile_context_t *ctx;
    mmpm_compile_result_t result = {0};
    mmpm_compile_index_list_t new_combinations = {0};
    uint32_t *active_ckeys = NULL;
    uint32_t active_ckey_count = 0;
    uint32_t i;
    int ret = MMPM_SUCCESS;

    if (handler == NULL || id_count == NULL ||
        (alias_count != 0 && aliases == NULL) ||
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

    for (i = 0; i < alias_count; i++) {
        const mmpm_compile_expr_t *expr = mmpm_compile_context_find_expr(ctx, aliases[i]);

        if (expr == NULL) {
            continue;
        }
        if ((expr->flags & MMPM_FLAG_QUIET) == 0) {
            ret = mmpm_compile_result_add(&result, expr->id);
            if (ret != MMPM_SUCCESS) {
                goto end;
            }
        }
    }

    ret = mmpm_compile_mark_aliases(ctx, aliases, alias_count, logical_vector, logical_count);
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
                                                logical_count,
                                                combination_vector,
                                                combination_count,
                                                exhaustion_vector,
                                                exhaustion_count,
                                                end_offset,
                                                &result,
                                                &new_combinations,
                                                true);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    if (final_flush) {
        ret = mmpm_compile_eval_final_combinations(ctx,
                                                   logical_vector,
                                                   logical_count,
                                                   combination_vector,
                                                   combination_count,
                                                   exhaustion_vector,
                                                   exhaustion_count,
                                                   end_offset,
                                                   &result,
                                                   &new_combinations,
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
    if (active_ckeys != NULL) {
        mmpm_free(active_ckeys);
    }
    mmpm_compile_index_list_destroy(&new_combinations);
    mmpm_compile_result_destroy(&result);
    return ret;
}
