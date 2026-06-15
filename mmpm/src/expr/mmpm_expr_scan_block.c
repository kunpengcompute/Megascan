/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mmpm.h"
#include "mmpm_ac.h"
#include "mmpm_expr.h"
#include "mmpm_expr_internal.h"

__attribute__((visibility("default")))
int mmpm_compile_scan_ac_combinations(mmpm_handler_t *handler,
                                      const uint8_t *data,
                                      uint32_t len,
                                      uint32_t *comb_ids,
                                      uint32_t *comb_count,
                                      uint32_t comb_capacity)
{
    mmpm_compile_context_t *ctx;
    uint32_t *ids = NULL;
    uint32_t id_count = 0;
    uint32_t id_capacity;
    uint32_t out_count = 0;
    uint32_t i;
    uint32_t j;
    int ret;

    if (handler == NULL || data == NULL || comb_count == NULL ||
        (comb_capacity != 0 && comb_ids == NULL) ||
        handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL || !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    *comb_count = 0;
    ctx = (mmpm_compile_context_t *)handler->mmpm_compile_ctx;
    if (ctx == NULL || ctx->comb_count == 0) {
        return MMPM_FIND_FAIL;
    }

    id_capacity = ctx->expr_count + ctx->comb_count;
    ids = (uint32_t *)mmpm_calloc(id_capacity, sizeof(uint32_t));
    if (ids == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    ret = mmpm_scan_expr(handler, data, len, ids, &id_count, id_capacity);
    if (ret != MMPM_SUCCESS && ret != MMPM_FIND_FAIL) {
        mmpm_free(ids);
        return ret;
    }

    for (i = 0; i < id_count; i++) {
        for (j = 0; j < ctx->comb_count; j++) {
            if (ids[i] != ctx->combs[j].id) {
                continue;
            }
            out_count++;
            if (out_count <= comb_capacity && comb_ids != NULL) {
                comb_ids[out_count - 1] = ids[i];
            }
            break;
        }
    }

    mmpm_free(ids);
    *comb_count = out_count;
    if (out_count > comb_capacity) {
        return MMPM_INVALID_PARA;
    }
    return out_count ? MMPM_SUCCESS : MMPM_FIND_FAIL;
}

__attribute__((visibility("default")))
int mmpm_scan_expr(mmpm_handler_t *handler, const uint8_t *data, uint32_t len,
                   uint32_t *ids, uint32_t *id_count, uint32_t id_capacity)
{
    mmpm_compile_context_t *ctx = NULL;
    mmpm_compile_alias_capture_t capture = {0};
    uint8_t *logical_vector = NULL;
    uint8_t *combination_vector = NULL;
    uint8_t *exhaustion_vector = NULL;
    uint32_t logical_count = 0;
    uint32_t combination_count = 0;
    uint32_t exhaustion_count = 0;
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
                                         &combination_count,
                                         &exhaustion_vector,
                                         &exhaustion_count);
    if (ret != MMPM_SUCCESS) {
        goto end;
    }

    ret = mmpm_compile_eval_stream_matches(handler,
                                           capture.aliases,
                                           capture.offsets,
                                           capture.count,
                                           logical_vector,
                                           logical_count,
                                           combination_vector,
                                           combination_count,
                                           exhaustion_vector,
                                           exhaustion_count,
                                           0,
                                           len,
                                           true,
                                           true,
                                           ids,
                                           id_count,
                                           id_capacity);

end:
    mmpm_compile_stream_state_destroy(logical_vector, combination_vector, exhaustion_vector);
    if (capture.aliases != NULL) {
        mmpm_free(capture.aliases);
    }
    if (capture.offsets != NULL) {
        mmpm_free(capture.offsets);
    }
    return ret;
}
