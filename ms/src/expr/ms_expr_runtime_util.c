/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ms_allocator.h"
#include "ms_expr_internal.h"
#include "ms_logical.h"

int ms_compile_bitset_get(const uint8_t *vector, uint32_t count, uint32_t key, uint8_t *value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (vector == NULL || value == NULL || key >= count) {
        return MS_INVALID_PARA;
    }

    byte_index = key / 8u;
    bit_index = key % 8u;
    *value = (vector[byte_index] & (uint8_t)(1u << bit_index)) ? 1u : 0u;
    return MS_SUCCESS;
}

int ms_compile_bitset_set(uint8_t *vector, uint32_t count, uint32_t key, uint8_t value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (vector == NULL || key >= count) {
        return MS_INVALID_PARA;
    }

    byte_index = key / 8u;
    bit_index = key % 8u;
    if (value != 0) {
        vector[byte_index] |= (uint8_t)(1u << bit_index);
    } else {
        vector[byte_index] &= (uint8_t)(~(1u << bit_index));
    }
    return MS_SUCCESS;
}

static bool ms_compile_result_has_id(const ms_compile_result_t *result, uint32_t id)
{
    uint32_t i;

    if (result == NULL) {
        return false;
    }

    for (i = 0; i < result->count; i++) {
        if (result->ids[i] == id) {
            return true;
        }
    }

    return false;
}

static int ms_compile_result_reserve(ms_compile_result_t *result)
{
    uint32_t *ids = NULL;
    uint32_t new_capacity;

    if (result->count < result->capacity) {
        return MS_SUCCESS;
    }

    new_capacity = result->capacity ? result->capacity * 2 : 4;
    ids = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
    if (ids == NULL) {
        return MS_MALLOC_FAIL;
    }

    if (result->ids != NULL) {
        (void)memcpy(ids, result->ids, result->count * sizeof(uint32_t));
        ms_free(result->ids);
    }

    result->ids = ids;
    result->capacity = new_capacity;
    return MS_SUCCESS;
}

int ms_compile_result_add(ms_compile_result_t *result, uint32_t id)
{
    int ret;

    if (result == NULL || ms_compile_result_has_id(result, id)) {
        return MS_SUCCESS;
    }

    ret = ms_compile_result_reserve(result);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    result->ids[result->count] = id;
    result->count++;
    return MS_SUCCESS;
}

static int ms_compile_index_list_add(ms_compile_index_list_t *list, uint32_t index)
{
    uint32_t *indexes;
    uint32_t i;
    uint32_t new_capacity;

    if (list == NULL) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < list->count; i++) {
        if (list->indexes[i] == index) {
            return MS_SUCCESS;
        }
    }

    if (list->count >= list->capacity) {
        new_capacity = list->capacity ? list->capacity * 2 : 4;
        indexes = (uint32_t *)ms_calloc(new_capacity, sizeof(uint32_t));
        if (indexes == NULL) {
            return MS_MALLOC_FAIL;
        }
        if (list->indexes != NULL) {
            (void)memcpy(indexes, list->indexes, list->count * sizeof(uint32_t));
            ms_free(list->indexes);
        }
        list->indexes = indexes;
        list->capacity = new_capacity;
    }

    list->indexes[list->count++] = index;
    return MS_SUCCESS;
}

static bool ms_compile_comb_offset_allows(const ms_compile_comb_t *comb,
                                            uint64_t end_offset)
{
    if (comb == NULL) {
        return false;
    }

    return end_offset >= comb->min_offset && end_offset <= comb->max_offset;
}

static int ms_compile_comb_was_reported(const ms_compile_comb_t *comb,
                                          uint8_t *combination_vector,
                                          uint32_t combination_count,
                                          uint8_t *exhaustion_vector,
                                          uint32_t exhaustion_count,
                                          bool use_combination_vector,
                                          uint8_t *reported)
{
    int ret;

    if (comb == NULL || reported == NULL) {
        return MS_INVALID_PARA;
    }

    *reported = 0;
    if (use_combination_vector) {
        ret = ms_compile_bitset_get(combination_vector, combination_count,
                                      comb->ckey, reported);
        if (ret != MS_SUCCESS || *reported) {
            return ret;
        }
    }

    if (comb->ekey != UINT32_MAX) {
        if (exhaustion_vector == NULL || comb->ekey >= exhaustion_count) {
            return MS_INVALID_PARA;
        }
        ret = ms_compile_bitset_get(exhaustion_vector, exhaustion_count,
                                      comb->ekey, reported);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    return MS_SUCCESS;
}

static int ms_compile_note_comb_report(const ms_compile_comb_t *comb,
                                         ms_compile_index_list_t *new_combinations,
                                         ms_compile_index_list_t *new_exhaustions,
                                         bool use_combination_vector)
{
    int ret;

    if (comb == NULL) {
        return MS_INVALID_PARA;
    }

    if (use_combination_vector) {
        ret = ms_compile_index_list_add(new_combinations, comb->ckey);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    if (comb->ekey != UINT32_MAX) {
        ret = ms_compile_index_list_add(new_exhaustions, comb->ekey);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }

    return MS_SUCCESS;
}

static int ms_compile_eval_one_combination(const ms_compile_context_t *ctx,
                                             uint32_t ckey,
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
                                             bool use_combination_vector)
{
    const ms_compile_comb_t *comb;
    uint8_t matched = 0;
    uint8_t reported = 0;
    int ret;

    if (ctx == NULL || ckey >= ctx->comb_count || result == NULL ||
        new_exhaustions == NULL || logical_vector == NULL) {
        return MS_INVALID_PARA;
    }

    comb = &ctx->combs[ckey];
    if (comb->ckey != ckey) {
        return MS_INVALID_PARA;
    }

    if (!ms_compile_comb_offset_allows(comb, end_offset)) {
        return MS_SUCCESS;
    }

    ret = ms_logical_eval_inplace(&comb->tree, logical_vector,
                                    logical_count, &matched);
    if (ret != MS_SUCCESS || !matched) {
        return ret;
    }
    if (final_logical_vector != NULL) {
        ret = ms_logical_eval_inplace(&comb->tree, final_logical_vector,
                                        logical_count, &matched);
        if (ret != MS_SUCCESS || !matched) {
            return ret;
        }
    }

    ret = ms_compile_comb_was_reported(comb,
                                         combination_vector,
                                         combination_count,
                                         exhaustion_vector,
                                         exhaustion_count,
                                         use_combination_vector,
                                         &reported);
    if (ret != MS_SUCCESS || reported) {
        return ret;
    }

    ret = ms_compile_result_add(result, comb->id);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    return ms_compile_note_comb_report(comb, new_combinations,
                                         new_exhaustions,
                                         use_combination_vector);
}

void ms_compile_result_destroy(ms_compile_result_t *result)
{
    if (result == NULL) {
        return;
    }
    if (result->ids != NULL) {
        ms_free(result->ids);
    }
    (void)memset(result, 0, sizeof(*result));
}

void ms_compile_index_list_destroy(ms_compile_index_list_t *list)
{
    if (list == NULL) {
        return;
    }
    if (list->indexes != NULL) {
        ms_free(list->indexes);
    }
    (void)memset(list, 0, sizeof(*list));
}

int ms_compile_context_get_ckeys_for_aliases(const ms_compile_context_t *ctx,
                                               const uint32_t *aliases,
                                               uint32_t alias_count,
                                               uint32_t **ckeys,
                                               uint32_t *ckey_count)
{
    uint8_t *active = NULL;
    uint32_t *out = NULL;
    uint32_t active_count = 0;
    uint32_t i;
    uint32_t j;
    int ret = MS_SUCCESS;

    if (ctx == NULL || ckeys == NULL || ckey_count == NULL ||
        (alias_count != 0 && aliases == NULL)) {
        return MS_INVALID_PARA;
    }

    *ckeys = NULL;
    *ckey_count = 0;
    if (ctx->comb_count == 0 || alias_count == 0) {
        return MS_SUCCESS;
    }

    active = (uint8_t *)ms_calloc(ms_compile_logical_byte_count(ctx->comb_count), sizeof(uint8_t));
    if (active == NULL) {
        return MS_MALLOC_FAIL;
    }

    for (i = 0; i < alias_count; i++) {
        uint32_t lkey;

        ret = ms_compile_context_find_lkey(ctx, aliases[i], &lkey);
        if (ret == MS_FIND_FAIL) {
            continue;
        }
        if (ret != MS_SUCCESS) {
            goto end;
        }
        if (lkey >= ctx->logical_count) {
            ret = MS_INVALID_PARA;
            goto end;
        }

        for (j = 0; j < ctx->lkey_to_ckey_counts[lkey]; j++) {
            ret = ms_compile_bitset_set(active, ctx->comb_count,
                                          ctx->lkey_to_ckeys[lkey][j], 1);
            if (ret != MS_SUCCESS) {
                goto end;
            }
        }
    }

    for (i = 0; i < ctx->comb_count; i++) {
        uint8_t is_active = 0;

        ret = ms_compile_bitset_get(active, ctx->comb_count, i, &is_active);
        if (ret != MS_SUCCESS) {
            goto end;
        }
        if (is_active) {
            active_count++;
        }
    }

    if (active_count != 0) {
        uint32_t out_count = 0;

        out = (uint32_t *)ms_calloc(active_count, sizeof(uint32_t));
        if (out == NULL) {
            ret = MS_MALLOC_FAIL;
            goto end;
        }

        for (i = 0; i < ctx->comb_count; i++) {
            uint8_t is_active = 0;

            ret = ms_compile_bitset_get(active, ctx->comb_count, i, &is_active);
            if (ret != MS_SUCCESS) {
                goto end;
            }
            if (is_active) {
                out[out_count++] = i;
            }
        }
    }

    *ckeys = out;
    *ckey_count = active_count;
    out = NULL;

end:
    if (active != NULL) {
        ms_free(active);
    }
    if (out != NULL) {
        ms_free(out);
    }
    return ret;
}

int ms_compile_context_collect_ckeys(const ms_compile_context_t *ctx,
                                       const uint32_t *aliases,
                                       uint32_t alias_count,
                                       uint8_t *active_vector,
                                       uint32_t active_count,
                                       uint32_t *ckeys,
                                       uint32_t ckey_capacity,
                                       uint32_t *ckey_count)
{
    uint32_t active_bytes;
    uint32_t i;
    uint32_t j;
    int ret;

    if (ctx == NULL || ckey_count == NULL || active_count != ctx->comb_count ||
        (alias_count != 0 && aliases == NULL) ||
        (ctx->comb_count != 0 && active_vector == NULL) ||
        (ckey_capacity != 0 && ckeys == NULL)) {
        return MS_INVALID_PARA;
    }

    *ckey_count = 0;
    active_bytes = ms_compile_logical_byte_count(active_count);
    if (active_bytes != 0) {
        (void)memset(active_vector, 0, active_bytes);
    }

    for (i = 0; i < alias_count; i++) {
        uint32_t lkey;

        ret = ms_compile_context_find_lkey(ctx, aliases[i], &lkey);
        if (ret == MS_FIND_FAIL) {
            continue;
        }
        if (ret != MS_SUCCESS || lkey >= ctx->logical_count) {
            return ret == MS_SUCCESS ? MS_INVALID_PARA : ret;
        }

        for (j = 0; j < ctx->lkey_to_ckey_counts[lkey]; j++) {
            ret = ms_compile_bitset_set(active_vector, active_count,
                                          ctx->lkey_to_ckeys[lkey][j], 1);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }
    }

    for (i = 0; i < active_count; i++) {
        uint8_t is_active = 0;

        ret = ms_compile_bitset_get(active_vector, active_count, i, &is_active);
        if (ret != MS_SUCCESS) {
            return ret;
        }
        if (!is_active) {
            continue;
        }
        if (*ckey_count >= ckey_capacity) {
            return MS_INVALID_PARA;
        }
        ckeys[(*ckey_count)++] = i;
    }

    return MS_SUCCESS;
}

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
                                          bool use_combination_vector)
{
    uint32_t i;
    int ret;

    if (ctx == NULL || logical_vector == NULL || result == NULL ||
        new_exhaustions == NULL ||
        (active_ckey_count != 0 && active_ckeys == NULL)) {
        return MS_INVALID_PARA;
    }

    new_exhaustions->count = 0;

    if (use_combination_vector &&
        (combination_count != ctx->comb_count ||
         (ctx->comb_count != 0 && combination_vector == NULL) ||
         new_combinations == NULL)) {
        return MS_INVALID_PARA;
    }
    if (ctx->ekey_count != 0 &&
        (exhaustion_vector == NULL || exhaustion_count != ctx->ekey_count)) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < active_ckey_count; i++) {
        ret = ms_compile_eval_one_combination(ctx, active_ckeys[i],
                                                logical_vector,
                                                logical_count,
                                                combination_vector,
                                                combination_count,
                                                exhaustion_vector,
                                                exhaustion_count,
                                                end_offset,
                                                final_logical_vector,
                                                result,
                                                new_combinations,
                                                new_exhaustions,
                                                use_combination_vector);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

    for (i = 0; i < new_exhaustions->count; i++) {
        ret = ms_compile_bitset_set(exhaustion_vector, exhaustion_count,
                                      new_exhaustions->indexes[i], 1);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

end:
    return ret;
}

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
                                         bool use_combination_vector)
{
    uint32_t i;
    int ret = MS_SUCCESS;

    if (ctx == NULL || logical_vector == NULL || result == NULL ||
        new_exhaustions == NULL) {
        return MS_INVALID_PARA;
    }

    new_exhaustions->count = 0;
    if (use_combination_vector &&
        (combination_count != ctx->comb_count ||
         (ctx->comb_count != 0 && combination_vector == NULL) ||
         new_combinations == NULL)) {
        return MS_INVALID_PARA;
    }
    if (ctx->ekey_count != 0 &&
        (exhaustion_vector == NULL || exhaustion_count != ctx->ekey_count)) {
        return MS_INVALID_PARA;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        if (!ctx->combs[i].can_match_without_active_lkey) {
            continue;
        }
        ret = ms_compile_eval_one_combination(ctx, i,
                                                logical_vector,
                                                logical_count,
                                                combination_vector,
                                                combination_count,
                                                exhaustion_vector,
                                                exhaustion_count,
                                                end_offset,
                                                NULL,
                                                result,
                                                new_combinations,
                                                new_exhaustions,
                                                use_combination_vector);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

    for (i = 0; i < new_exhaustions->count; i++) {
        ret = ms_compile_bitset_set(exhaustion_vector, exhaustion_count,
                                      new_exhaustions->indexes[i], 1);
        if (ret != MS_SUCCESS) {
            goto end;
        }
    }

end:
    return ret;
}
