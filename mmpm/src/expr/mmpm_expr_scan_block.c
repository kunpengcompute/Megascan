/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <stdbool.h>
#include <stdint.h>

#include "mmpm.h"
#include "mmpm_expr.h"
#include "mmpm_runtime.h"
#include "mmpm_expr_internal.h"
#include "../mmpm_database_internal.h"

static int mmpm_scan_expr_via_database(mmpm_handler_t *handler,
                                       const uint8_t *data,
                                       uint32_t len,
                                       uint32_t *ids,
                                       uint32_t *id_count,
                                       uint32_t id_capacity)
{
    mmpm_database_t db_view = {0};
    mmpm_scratch_t *scratch = NULL;
    int ret;
    int free_ret;

    if (handler == NULL || data == NULL || id_count == NULL ||
        (id_capacity != 0 && ids == NULL) ||
        handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL || !handler->is_built ||
        handler->mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    *id_count = 0;
    mmpm_database_from_handler(&db_view, handler);
    ret = mmpm_alloc_scratch(&db_view, &scratch);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_scan_database(&db_view, data, len, scratch, ids, id_count, id_capacity);
    free_ret = mmpm_free_scratch(scratch);
    if ((ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) && free_ret != MMPM_SUCCESS) {
        return free_ret;
    }

    return ret;
}

static int mmpm_collect_combination_ids_from_matches(const mmpm_database_t *db,
                                                     const uint32_t *ids,
                                                     uint32_t id_count,
                                                     uint32_t *comb_ids,
                                                     uint32_t *comb_count,
                                                     uint32_t comb_capacity)
{
    const mmpm_compile_context_t *ctx;
    uint32_t out_count = 0;
    uint32_t i;
    uint32_t j;

    if (db == NULL || comb_count == NULL ||
        (id_count != 0 && ids == NULL) ||
        (comb_capacity != 0 && comb_ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    *comb_count = 0;
    ctx = (const mmpm_compile_context_t *)db->mmpm_compile_ctx;
    if (ctx == NULL || ctx->comb_count == 0) {
        return MMPM_FIND_FAIL;
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
        return MMPM_INVALID_PARA;
    }
    return out_count ? MMPM_SUCCESS : MMPM_FIND_FAIL;
}

__attribute__((visibility("default")))
int mmpm_compile_scan_ac_combinations(mmpm_handler_t *handler,
                                      const uint8_t *data,
                                      uint32_t len,
                                      uint32_t *comb_ids,
                                      uint32_t *comb_count,
                                      uint32_t comb_capacity)
{
    mmpm_database_t db_view = {0};
    mmpm_compile_context_t *ctx;
    uint32_t *ids = NULL;
    uint32_t id_count = 0;
    uint32_t id_capacity;
    int ret;

    if (handler == NULL || data == NULL || comb_count == NULL ||
        (comb_capacity != 0 && comb_ids == NULL) ||
        handler->type != MMPM_MATCHER_TYPE_AC ||
        handler->mmpm_matcher == NULL || !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    *comb_count = 0;
    mmpm_database_from_handler(&db_view, handler);
    ctx = (mmpm_compile_context_t *)db_view.mmpm_compile_ctx;
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

    ret = mmpm_collect_combination_ids_from_matches(&db_view,
                                                    ids,
                                                    id_count,
                                                    comb_ids,
                                                    comb_count,
                                                    comb_capacity);
    mmpm_free(ids);
    return ret;
}

__attribute__((visibility("default")))
int mmpm_scan_expr(mmpm_handler_t *handler, const uint8_t *data, uint32_t len,
                   uint32_t *ids, uint32_t *id_count, uint32_t id_capacity)
{
    return mmpm_scan_expr_via_database(handler, data, len, ids, id_count, id_capacity);
}
