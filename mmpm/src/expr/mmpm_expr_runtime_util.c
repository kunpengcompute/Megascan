/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mmpm_allocator.h"
#include "mmpm_expr_internal.h"
#include "mmpm_logical.h"

int mmpm_compile_bitset_get(const uint8_t *vector, uint32_t count, uint32_t key, uint8_t *value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (vector == NULL || value == NULL || key >= count) {
        return MMPM_INVALID_PARA;
    }

    byte_index = key / 8u;
    bit_index = key % 8u;
    *value = (vector[byte_index] & (uint8_t)(1u << bit_index)) ? 1u : 0u;
    return MMPM_SUCCESS;
}

int mmpm_compile_bitset_set(uint8_t *vector, uint32_t count, uint32_t key, uint8_t value)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (vector == NULL || key >= count) {
        return MMPM_INVALID_PARA;
    }

    byte_index = key / 8u;
    bit_index = key % 8u;
    if (value != 0) {
        vector[byte_index] |= (uint8_t)(1u << bit_index);
    } else {
        vector[byte_index] &= (uint8_t)(~(1u << bit_index));
    }
    return MMPM_SUCCESS;
}

static bool mmpm_compile_result_has_id(const mmpm_compile_result_t *result, uint32_t id)
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

static int mmpm_compile_result_reserve(mmpm_compile_result_t *result)
{
    uint32_t *ids = NULL;
    uint32_t new_capacity;

    if (result->count < result->capacity) {
        return MMPM_SUCCESS;
    }

    new_capacity = result->capacity ? result->capacity * 2 : 4;
    ids = (uint32_t *)mmpm_calloc(new_capacity, sizeof(uint32_t));
    if (ids == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    if (result->ids != NULL) {
        (void)memcpy(ids, result->ids, result->count * sizeof(uint32_t));
        mmpm_free(result->ids);
    }

    result->ids = ids;
    result->capacity = new_capacity;
    return MMPM_SUCCESS;
}

int mmpm_compile_result_add(mmpm_compile_result_t *result, uint32_t id)
{
    int ret;

    if (result == NULL || mmpm_compile_result_has_id(result, id)) {
        return MMPM_SUCCESS;
    }

    ret = mmpm_compile_result_reserve(result);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    result->ids[result->count] = id;
    result->count++;
    return MMPM_SUCCESS;
}

static int mmpm_compile_index_list_add(mmpm_compile_index_list_t *list, uint32_t index)
{
    uint32_t *indexes;
    uint32_t new_capacity;

    if (list == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (list->count >= list->capacity) {
        new_capacity = list->capacity ? list->capacity * 2 : 4;
        indexes = (uint32_t *)mmpm_calloc(new_capacity, sizeof(uint32_t));
        if (indexes == NULL) {
            return MMPM_MALLOC_FAIL;
        }
        if (list->indexes != NULL) {
            (void)memcpy(indexes, list->indexes, list->count * sizeof(uint32_t));
            mmpm_free(list->indexes);
        }
        list->indexes = indexes;
        list->capacity = new_capacity;
    }

    list->indexes[list->count++] = index;
    return MMPM_SUCCESS;
}

static bool mmpm_compile_comb_offset_allows(const mmpm_compile_comb_t *comb,
                                            uint64_t end_offset)
{
    if (comb == NULL) {
        return false;
    }

    return end_offset >= comb->min_offset && end_offset <= comb->max_offset;
}

static int mmpm_compile_comb_was_reported(const mmpm_compile_comb_t *comb,
                                          uint8_t *combination_vector,
                                          uint32_t combination_count,
                                          uint8_t *exhaustion_vector,
                                          uint32_t exhaustion_count,
                                          bool use_combination_vector,
                                          uint8_t *reported)
{
    int ret;

    if (comb == NULL || reported == NULL) {
        return MMPM_INVALID_PARA;
    }

    *reported = 0;
    if (use_combination_vector) {
        ret = mmpm_compile_bitset_get(combination_vector, combination_count,
                                      comb->ckey, reported);
        if (ret != MMPM_SUCCESS || *reported) {
            return ret;
        }
    }

    if (comb->ekey != UINT32_MAX) {
        if (exhaustion_vector == NULL || comb->ekey >= exhaustion_count) {
            return MMPM_INVALID_PARA;
        }
        ret = mmpm_compile_bitset_get(exhaustion_vector, exhaustion_count,
                                      comb->ekey, reported);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_note_comb_report(const mmpm_compile_comb_t *comb,
                                         mmpm_compile_index_list_t *new_combinations,
                                         mmpm_compile_index_list_t *new_exhaustions,
                                         bool use_combination_vector)
{
    int ret;

    if (comb == NULL) {
        return MMPM_INVALID_PARA;
    }

    if (use_combination_vector) {
        ret = mmpm_compile_index_list_add(new_combinations, comb->ckey);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    if (comb->ekey != UINT32_MAX) {
        ret = mmpm_compile_index_list_add(new_exhaustions, comb->ekey);
        if (ret != MMPM_SUCCESS) {
            return ret;
        }
    }

    return MMPM_SUCCESS;
}

static int mmpm_compile_eval_one_combination(const mmpm_compile_context_t *ctx,
                                             uint32_t ckey,
                                             uint8_t *logical_vector,
                                             uint32_t logical_count,
                                             uint8_t *combination_vector,
                                             uint32_t combination_count,
                                             uint8_t *exhaustion_vector,
                                             uint32_t exhaustion_count,
                                             uint64_t end_offset,
                                             uint8_t *final_logical_vector,
                                             mmpm_compile_result_t *result,
                                             mmpm_compile_index_list_t *new_combinations,
                                             mmpm_compile_index_list_t *new_exhaustions,
                                             bool use_combination_vector)
{
    const mmpm_compile_comb_t *comb;
    uint8_t matched = 0;
    uint8_t reported = 0;
    int ret;

    if (ctx == NULL || ckey >= ctx->comb_count || result == NULL ||
        new_exhaustions == NULL || logical_vector == NULL) {
        return MMPM_INVALID_PARA;
    }

    comb = &ctx->combs[ckey];
    if (comb->ckey != ckey) {
        return MMPM_INVALID_PARA;
    }

    if (!mmpm_compile_comb_offset_allows(comb, end_offset)) {
        return MMPM_SUCCESS;
    }

    ret = mmpm_logical_eval_inplace(&comb->tree, logical_vector,
                                    logical_count, &matched);
    if (ret != MMPM_SUCCESS || !matched) {
        return ret;
    }
    if (final_logical_vector != NULL) {
        ret = mmpm_logical_eval_inplace(&comb->tree, final_logical_vector,
                                        logical_count, &matched);
        if (ret != MMPM_SUCCESS || !matched) {
            return ret;
        }
    }

    ret = mmpm_compile_comb_was_reported(comb,
                                         combination_vector,
                                         combination_count,
                                         exhaustion_vector,
                                         exhaustion_count,
                                         use_combination_vector,
                                         &reported);
    if (ret != MMPM_SUCCESS || reported) {
        return ret;
    }

    ret = mmpm_compile_result_add(result, comb->id);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    return mmpm_compile_note_comb_report(comb, new_combinations,
                                         new_exhaustions,
                                         use_combination_vector);
}

void mmpm_compile_result_destroy(mmpm_compile_result_t *result)
{
    if (result == NULL) {
        return;
    }
    if (result->ids != NULL) {
        mmpm_free(result->ids);
    }
    (void)memset(result, 0, sizeof(*result));
}

void mmpm_compile_index_list_destroy(mmpm_compile_index_list_t *list)
{
    if (list == NULL) {
        return;
    }
    if (list->indexes != NULL) {
        mmpm_free(list->indexes);
    }
    (void)memset(list, 0, sizeof(*list));
}

int mmpm_compile_context_get_ckeys_for_aliases(const mmpm_compile_context_t *ctx,
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
    int ret = MMPM_SUCCESS;

    if (ctx == NULL || ckeys == NULL || ckey_count == NULL ||
        (alias_count != 0 && aliases == NULL)) {
        return MMPM_INVALID_PARA;
    }

    *ckeys = NULL;
    *ckey_count = 0;
    if (ctx->comb_count == 0 || alias_count == 0) {
        return MMPM_SUCCESS;
    }

    active = (uint8_t *)mmpm_calloc(mmpm_compile_logical_byte_count(ctx->comb_count), sizeof(uint8_t));
    if (active == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    for (i = 0; i < alias_count; i++) {
        uint32_t lkey;

        ret = mmpm_compile_context_find_lkey(ctx, aliases[i], &lkey);
        if (ret == MMPM_FIND_FAIL) {
            continue;
        }
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
        if (lkey >= ctx->logical_count) {
            ret = MMPM_INVALID_PARA;
            goto end;
        }

        for (j = 0; j < ctx->lkey_to_ckey_counts[lkey]; j++) {
            ret = mmpm_compile_bitset_set(active, ctx->comb_count,
                                          ctx->lkey_to_ckeys[lkey][j], 1);
            if (ret != MMPM_SUCCESS) {
                goto end;
            }
        }
    }

    for (i = 0; i < ctx->comb_count; i++) {
        uint8_t is_active = 0;

        ret = mmpm_compile_bitset_get(active, ctx->comb_count, i, &is_active);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
        if (is_active) {
            active_count++;
        }
    }

    if (active_count != 0) {
        uint32_t out_count = 0;

        out = (uint32_t *)mmpm_calloc(active_count, sizeof(uint32_t));
        if (out == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }

        for (i = 0; i < ctx->comb_count; i++) {
            uint8_t is_active = 0;

            ret = mmpm_compile_bitset_get(active, ctx->comb_count, i, &is_active);
            if (ret != MMPM_SUCCESS) {
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
        mmpm_free(active);
    }
    if (out != NULL) {
        mmpm_free(out);
    }
    return ret;
}

int mmpm_compile_eval_active_combinations(const mmpm_compile_context_t *ctx,
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
                                          mmpm_compile_result_t *result,
                                          mmpm_compile_index_list_t *new_combinations,
                                          bool use_combination_vector)
{
    uint32_t i;
    int ret;
    mmpm_compile_index_list_t new_exhaustions = {0};

    if (ctx == NULL || logical_vector == NULL || result == NULL ||
        (active_ckey_count != 0 && active_ckeys == NULL)) {
        return MMPM_INVALID_PARA;
    }

    if (use_combination_vector &&
        (combination_count != ctx->comb_count ||
         (ctx->comb_count != 0 && combination_vector == NULL) ||
         new_combinations == NULL)) {
        return MMPM_INVALID_PARA;
    }
    if (ctx->ekey_count != 0 &&
        (exhaustion_vector == NULL || exhaustion_count != ctx->ekey_count)) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < active_ckey_count; i++) {
        ret = mmpm_compile_eval_one_combination(ctx, active_ckeys[i],
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
                                                &new_exhaustions,
                                                use_combination_vector);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

    for (i = 0; i < new_exhaustions.count; i++) {
        ret = mmpm_compile_bitset_set(exhaustion_vector, exhaustion_count,
                                      new_exhaustions.indexes[i], 1);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

end:
    mmpm_compile_index_list_destroy(&new_exhaustions);
    return ret;
}

int mmpm_compile_eval_final_combinations(const mmpm_compile_context_t *ctx,
                                         uint8_t *logical_vector,
                                         uint32_t logical_count,
                                         uint8_t *combination_vector,
                                         uint32_t combination_count,
                                         uint8_t *exhaustion_vector,
                                         uint32_t exhaustion_count,
                                         uint64_t end_offset,
                                         mmpm_compile_result_t *result,
                                         mmpm_compile_index_list_t *new_combinations,
                                         bool use_combination_vector)
{
    uint32_t i;
    int ret = MMPM_SUCCESS;
    mmpm_compile_index_list_t new_exhaustions = {0};

    if (ctx == NULL || logical_vector == NULL || result == NULL) {
        return MMPM_INVALID_PARA;
    }
    if (use_combination_vector &&
        (combination_count != ctx->comb_count ||
         (ctx->comb_count != 0 && combination_vector == NULL) ||
         new_combinations == NULL)) {
        return MMPM_INVALID_PARA;
    }
    if (ctx->ekey_count != 0 &&
        (exhaustion_vector == NULL || exhaustion_count != ctx->ekey_count)) {
        return MMPM_INVALID_PARA;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        if (!ctx->combs[i].can_match_without_active_lkey) {
            continue;
        }
        ret = mmpm_compile_eval_one_combination(ctx, i,
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
                                                &new_exhaustions,
                                                use_combination_vector);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

    for (i = 0; i < new_exhaustions.count; i++) {
        ret = mmpm_compile_bitset_set(exhaustion_vector, exhaustion_count,
                                      new_exhaustions.indexes[i], 1);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
    }

end:
    mmpm_compile_index_list_destroy(&new_exhaustions);
    return ret;
}
