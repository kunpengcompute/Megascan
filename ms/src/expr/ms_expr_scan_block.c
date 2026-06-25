/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>

#include "ms.h"
#include "ms_expr.h"
#include "ms_runtime.h"
#include "ms_expr_internal.h"
#include "../ms_database_internal.h"

static int ms_scan_expr_via_database(ms_handler_t *handler,
                                       const uint8_t *data,
                                       uint32_t len,
                                       uint32_t *ids,
                                       uint32_t *id_count,
                                       uint32_t id_capacity)
{
    ms_database_t db_view = {0};
    ms_scratch_t *scratch = NULL;
    int ret;
    int free_ret;

    if (handler == NULL || data == NULL || id_count == NULL ||
        (id_capacity != 0 && ids == NULL) ||
        handler->type != MS_MATCHER_TYPE_AC ||
        handler->ms_matcher == NULL || !handler->is_built ||
        handler->ms_compile_ctx == NULL) {
        return MS_INVALID_PARA;
    }

    *id_count = 0;
    ms_database_from_handler(&db_view, handler);
    ret = ms_alloc_scratch(&db_view, &scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    ret = ms_scan_database(&db_view, data, len, scratch, ids, id_count, id_capacity);
    free_ret = ms_free_scratch(scratch);
    if ((ret == MS_SUCCESS || ret == MS_FIND_FAIL) && free_ret != MS_SUCCESS) {
        return free_ret;
    }

    return ret;
}

static int ms_collect_combination_ids_from_matches(const ms_database_t *db,
                                                     const uint32_t *ids,
                                                     uint32_t id_count,
                                                     uint32_t *comb_ids,
                                                     uint32_t *comb_count,
                                                     uint32_t comb_capacity)
{
    const ms_compile_context_t *ctx;
    uint32_t out_count = 0;
    uint32_t i;
    uint32_t j;

    if (db == NULL || comb_count == NULL ||
        (id_count != 0 && ids == NULL) ||
        (comb_capacity != 0 && comb_ids == NULL)) {
        return MS_INVALID_PARA;
    }

    *comb_count = 0;
    ctx = (const ms_compile_context_t *)db->ms_compile_ctx;
    if (ctx == NULL || ctx->comb_count == 0) {
        return MS_FIND_FAIL;
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

    *comb_count = out_count;
    if (out_count > comb_capacity) {
        return MS_INVALID_PARA;
    }
    return out_count ? MS_SUCCESS : MS_FIND_FAIL;
}

__attribute__((visibility("default")))
int ms_compile_scan_ac_combinations(ms_handler_t *handler,
                                      const uint8_t *data,
                                      uint32_t len,
                                      uint32_t *comb_ids,
                                      uint32_t *comb_count,
                                      uint32_t comb_capacity)
{
    ms_database_t db_view = {0};
    ms_compile_context_t *ctx;
    uint32_t *ids = NULL;
    uint32_t id_count = 0;
    uint32_t id_capacity;
    int ret;

    if (handler == NULL || data == NULL || comb_count == NULL ||
        (comb_capacity != 0 && comb_ids == NULL) ||
        handler->type != MS_MATCHER_TYPE_AC ||
        handler->ms_matcher == NULL || !handler->is_built) {
        return MS_INVALID_PARA;
    }

    *comb_count = 0;
    ms_database_from_handler(&db_view, handler);
    ctx = (ms_compile_context_t *)db_view.ms_compile_ctx;
    if (ctx == NULL || ctx->comb_count == 0) {
        return MS_FIND_FAIL;
    }

    id_capacity = ctx->expr_count + ctx->comb_count;
    ids = (uint32_t *)ms_calloc(id_capacity, sizeof(uint32_t));
    if (ids == NULL) {
        return MS_MALLOC_FAIL;
    }

    ret = ms_scan_expr(handler, data, len, ids, &id_count, id_capacity);
    if (ret != MS_SUCCESS && ret != MS_FIND_FAIL) {
        ms_free(ids);
        return ret;
    }

    ret = ms_collect_combination_ids_from_matches(&db_view,
                                                    ids,
                                                    id_count,
                                                    comb_ids,
                                                    comb_count,
                                                    comb_capacity);
    ms_free(ids);
    return ret;
}

__attribute__((visibility("default")))
int ms_scan_expr(ms_handler_t *handler, const uint8_t *data, uint32_t len,
                   uint32_t *ids, uint32_t *id_count, uint32_t id_capacity)
{
    return ms_scan_expr_via_database(handler, data, len, ids, id_count, id_capacity);
}
