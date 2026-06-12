/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mmpm.h"
#include "mmpm_ac.h"
#include "mmpm_allocator.h"
#include "mmpm_expr.h"
#include "mmpm_expr_internal.h"
#include "mmpm_logical.h"

typedef struct {
    uint32_t *aliases;
    uint32_t count;
    uint32_t capacity;
} mmpm_compile_alias_capture_t;

typedef struct {
    uint32_t *ids;
    uint32_t count;
    uint32_t capacity;
} mmpm_compile_result_t;

typedef struct {
    uint32_t *indexes;
    uint32_t count;
    uint32_t capacity;
} mmpm_compile_index_list_t;

static int mmpm_compile_bitset_get(const uint8_t *vector, uint32_t count, uint32_t key, uint8_t *value)
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

static int mmpm_compile_bitset_set(uint8_t *vector, uint32_t count, uint32_t key, uint8_t value)
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

static int mmpm_compile_alias_capture_reserve(mmpm_compile_alias_capture_t *capture)
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

static int mmpm_compile_capture_alias(uint32_t alias, void *ctx)
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

static int mmpm_compile_capture_alias_unique(uint32_t alias, void *ctx)
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

static int mmpm_compile_result_add(mmpm_compile_result_t *result, uint32_t id)
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
    uint32_t i;
    uint32_t matched_count = 0;
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

    ret = mmpm_compile_mark_aliases(ctx, aliases, alias_count, logical_vector, ctx->logical_count);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    for (i = 0; i < ctx->comb_count; i++) {
        uint8_t matched = 0;

        ret = mmpm_logical_eval(&ctx->combs[i].tree, logical_vector, ctx->logical_count, &matched);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }

        if (!matched) {
            continue;
        }

        if (matched_count < comb_capacity) {
            comb_ids[matched_count] = ctx->combs[i].id;
        }
        matched_count++;
    }

    *comb_count = matched_count;
    if (matched_count > comb_capacity) {
        ret = MMPM_INVALID_PARA;
    }

end:
    if (logical_vector != NULL) {
        mmpm_free(logical_vector);
    }
    return ret;
}

int mmpm_compile_stream_state_init(mmpm_handler_t *handler,
                                   uint8_t **logical_vector,
                                   uint32_t *logical_count,
                                   uint8_t **combination_vector,
                                   uint32_t *combination_count)
{
    mmpm_compile_context_t *ctx;
    uint32_t logical_bytes = 0;
    uint32_t combination_bytes = 0;

    if (handler == NULL || logical_vector == NULL || logical_count == NULL ||
        combination_vector == NULL || combination_count == NULL) {
        return MMPM_INVALID_PARA;
    }

    *logical_vector = NULL;
    *logical_count = 0;
    *combination_vector = NULL;
    *combination_count = 0;

    ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    if (ctx == NULL) {
        return MMPM_SUCCESS;
    }

    *logical_count = ctx->logical_count;
    *combination_count = ctx->comb_count;
    logical_bytes = mmpm_compile_logical_byte_count(ctx->logical_count);
    combination_bytes = mmpm_compile_logical_byte_count(ctx->comb_count);

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

    return MMPM_SUCCESS;
}

void mmpm_compile_stream_state_destroy(uint8_t *logical_vector,
                                       uint8_t *combination_vector)
{
    if (logical_vector != NULL) {
        mmpm_free(logical_vector);
    }
    if (combination_vector != NULL) {
        mmpm_free(combination_vector);
    }
}

int mmpm_compile_eval_stream_matches(mmpm_handler_t *handler,
                                     const uint32_t *aliases,
                                     uint32_t alias_count,
                                     uint8_t *logical_vector,
                                     uint32_t logical_count,
                                     uint8_t *combination_vector,
                                     uint32_t combination_count,
                                     uint32_t *ids,
                                     uint32_t *id_count,
                                     uint32_t id_capacity)
{
    mmpm_compile_context_t *ctx;
    mmpm_compile_result_t result = {0};
    mmpm_compile_index_list_t new_combinations = {0};
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
        (ctx->comb_count != 0 && (combination_vector == NULL || combination_count != ctx->comb_count))) {
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

    for (i = 0; i < ctx->comb_count; i++) {
        uint8_t matched = 0;
        uint8_t reported = 0;

        ret = mmpm_logical_eval(&ctx->combs[i].tree, logical_vector, logical_count, &matched);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
        if (!matched) {
            continue;
        }

        ret = mmpm_compile_bitset_get(combination_vector, combination_count, i, &reported);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }
        if (reported) {
            continue;
        }

        ret = mmpm_compile_result_add(&result, ctx->combs[i].id);
        if (ret != MMPM_SUCCESS) {
            goto end;
        }

        ret = mmpm_compile_index_list_add(&new_combinations, i);
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
    if (new_combinations.indexes != NULL) {
        mmpm_free(new_combinations.indexes);
    }
    if (result.ids != NULL) {
        mmpm_free(result.ids);
    }
    return ret;
}

__attribute__((visibility("default")))
int mmpm_compile_scan_ac_combinations(mmpm_handler_t *handler,
                                      const uint8_t *data,
                                      uint32_t len,
                                      uint32_t *comb_ids,
                                      uint32_t *comb_count,
                                      uint32_t comb_capacity)
{
    mmpm_compile_alias_capture_t capture = {0};
    uint32_t node_index = 0;
    int scan_ret;
    int ret;

    if (handler == NULL || data == NULL || comb_count == NULL ||
        handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL || !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    *comb_count = 0;
    scan_ret = ac_automaton_match_state_cb((ACAutomaton *)handler->mmpm_matcher,
                                           data, len, &node_index,
                                           mmpm_compile_capture_alias,
                                           &capture);
    if (scan_ret != MMPM_SUCCESS && scan_ret != MMPM_FIND_FAIL) {
        ret = scan_ret;
        goto end;
    }

    ret = mmpm_compile_eval_combinations(handler, capture.aliases, capture.count,
                                         comb_ids, comb_count, comb_capacity);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    ret = (*comb_count == 0) ? MMPM_FIND_FAIL : MMPM_SUCCESS;

end:
    if (capture.aliases != NULL) {
        mmpm_free(capture.aliases);
    }
    return ret;
}

__attribute__((visibility("default")))
int mmpm_scan_expr(mmpm_handler_t *handler, const uint8_t *data, uint32_t len,
                   uint32_t *ids, uint32_t *id_count, uint32_t id_capacity)
{
    mmpm_compile_context_t *ctx = NULL;
    mmpm_compile_alias_capture_t capture = {0};
    uint8_t *logical_vector = NULL;
    uint8_t *combination_vector = NULL;
    uint32_t logical_count = 0;
    uint32_t combination_count = 0;
    uint32_t node_index = 0;
    int scan_ret;
    int ret;

    if (handler == NULL || data == NULL || id_count == NULL ||
        (id_capacity != 0 && ids == NULL) ||
        handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL || !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    *id_count = 0;
    ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    if (ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    scan_ret = ac_automaton_match_state_cb((ACAutomaton *)handler->mmpm_matcher,
                                           data, len, &node_index,
                                           mmpm_compile_capture_alias,
                                           &capture);
    if (scan_ret != MMPM_SUCCESS && scan_ret != MMPM_FIND_FAIL) {
        ret = scan_ret;
        goto end;
    }

    ret = mmpm_compile_stream_state_init(handler,
                                         &logical_vector,
                                         &logical_count,
                                         &combination_vector,
                                         &combination_count);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    ret = mmpm_compile_eval_stream_matches(handler,
                                           capture.aliases,
                                           capture.count,
                                           logical_vector,
                                           logical_count,
                                           combination_vector,
                                           combination_count,
                                           ids,
                                           id_count,
                                           id_capacity);

end:
    mmpm_compile_stream_state_destroy(logical_vector, combination_vector);
    if (capture.aliases != NULL) {
        mmpm_free(capture.aliases);
    }
    return ret;
}

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
                          uint32_t id_capacity)
{
    mmpm_compile_alias_capture_t capture = {0};
    uint32_t next_node_index;
    uint8_t *logical_vector_copy = NULL;
    uint8_t *combination_vector_copy = NULL;
    uint32_t logical_bytes;
    uint32_t combination_bytes;
    int scan_ret;
    int ret;

    if (handler == NULL || automaton == NULL || node_index == NULL || offset == NULL ||
        data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    next_node_index = *node_index;
    logical_bytes = mmpm_compile_logical_byte_count(logical_count);
    combination_bytes = mmpm_compile_logical_byte_count(combination_count);
    if (logical_bytes != 0) {
        logical_vector_copy = (uint8_t *)mmpm_calloc(logical_bytes, sizeof(uint8_t));
        if (logical_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(logical_vector_copy, logical_vector, logical_bytes);
    }
    if (combination_bytes != 0) {
        combination_vector_copy = (uint8_t *)mmpm_calloc(combination_bytes, sizeof(uint8_t));
        if (combination_vector_copy == NULL) {
            ret = MMPM_MALLOC_FAIL;
            goto end;
        }
        (void)memcpy(combination_vector_copy, combination_vector, combination_bytes);
    }

    *id_count = 0;
    scan_ret = ac_automaton_match_state_cb(automaton,
                                           data,
                                           len,
                                           &next_node_index,
                                           mmpm_compile_capture_alias_unique,
                                           &capture);
    if (scan_ret != MMPM_SUCCESS && scan_ret != MMPM_FIND_FAIL) {
        ret = scan_ret;
        goto end;
    }

    ret = mmpm_compile_eval_stream_matches(handler,
                                           capture.aliases,
                                           capture.count,
                                           logical_vector_copy,
                                           logical_count,
                                           combination_vector_copy,
                                           combination_count,
                                           ids,
                                           id_count,
                                           id_capacity);
    if (ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) {
        *node_index = next_node_index;
        *offset += len;
        if (logical_bytes != 0) {
            (void)memcpy(logical_vector, logical_vector_copy, logical_bytes);
        }
        if (combination_bytes != 0) {
            (void)memcpy(combination_vector, combination_vector_copy, combination_bytes);
        }
    }

end:
    if (logical_vector_copy != NULL) {
        mmpm_free(logical_vector_copy);
    }
    if (combination_vector_copy != NULL) {
        mmpm_free(combination_vector_copy);
    }
    if (capture.aliases != NULL) {
        mmpm_free(capture.aliases);
    }
    return ret;
}
