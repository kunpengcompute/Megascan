/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <string.h>

#include "mmpm_database.h"
#include "mmpm_ac.h"
#include "mmpm_allocator.h"
#include "mmpm_database_internal.h"
#include "expr/mmpm_expr_internal.h"

struct mmpm_scratch {
    const mmpm_database_t *db;
    uint8_t *logical_vector;
    uint32_t logical_count;
    uint8_t *combination_vector;
    uint32_t combination_count;
    uint8_t *exhaustion_vector;
    uint32_t exhaustion_count;
    uint32_t *aliases;
    uint64_t *offsets;
    uint32_t capture_count;
    uint32_t capture_capacity;
};

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_alloc_scratch(const mmpm_database_t *db, mmpm_scratch_t **scratch)
{
    mmpm_scratch_t *new_scratch;
    mmpm_handler_t scratch_handler = {0};
    int ret;

    if (db == NULL || scratch == NULL || *scratch != NULL) {
        return MMPM_INVALID_PARA;
    }

    new_scratch = (mmpm_scratch_t *)mmpm_scratch_calloc(1, sizeof(mmpm_scratch_t));
    if (new_scratch == NULL) {
        return MMPM_MALLOC_FAIL;
    }

    new_scratch->db = db;
    mmpm_database_to_handler(db, &scratch_handler);
    ret = mmpm_compile_scratch_state_init(&scratch_handler,
                                          &new_scratch->logical_vector,
                                          &new_scratch->logical_count,
                                          &new_scratch->combination_vector,
                                          &new_scratch->combination_count,
                                          &new_scratch->exhaustion_vector,
                                          &new_scratch->exhaustion_count);
    if (ret != MMPM_SUCCESS) {
        mmpm_scratch_free(new_scratch);
        return ret;
    }

    *scratch = new_scratch;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_free_scratch(mmpm_scratch_t *scratch)
{
    if (scratch == NULL) {
        return MMPM_INVALID_PARA;
    }

    mmpm_compile_scratch_state_destroy(scratch->logical_vector,
                                       scratch->combination_vector,
                                       scratch->exhaustion_vector);
    if (scratch->aliases != NULL) {
        mmpm_scratch_free(scratch->aliases);
    }
    if (scratch->offsets != NULL) {
        mmpm_scratch_free(scratch->offsets);
    }
    mmpm_scratch_free(scratch);
    return MMPM_SUCCESS;
}

static void mmpm_scratch_clear_state(mmpm_scratch_t *scratch)
{
    uint32_t bytes;

    bytes = mmpm_compile_logical_byte_count(scratch->logical_count);
    if (scratch->logical_vector != NULL && bytes != 0) {
        (void)memset(scratch->logical_vector, 0, bytes);
    }

    bytes = mmpm_compile_logical_byte_count(scratch->combination_count);
    if (scratch->combination_vector != NULL && bytes != 0) {
        (void)memset(scratch->combination_vector, 0, bytes);
    }

    bytes = mmpm_compile_logical_byte_count(scratch->exhaustion_count);
    if (scratch->exhaustion_vector != NULL && bytes != 0) {
        (void)memset(scratch->exhaustion_vector, 0, bytes);
    }
}

static int mmpm_scratch_capture_reserve(mmpm_scratch_t *scratch)
{
    uint32_t *aliases = NULL;
    uint64_t *offsets = NULL;
    uint32_t new_capacity;

    if (scratch->capture_count < scratch->capture_capacity) {
        return MMPM_SUCCESS;
    }

    new_capacity = scratch->capture_capacity ? scratch->capture_capacity * 2 : 4;
    aliases = (uint32_t *)mmpm_scratch_calloc(new_capacity, sizeof(uint32_t));
    offsets = (uint64_t *)mmpm_scratch_calloc(new_capacity, sizeof(uint64_t));
    if (aliases == NULL || offsets == NULL) {
        if (aliases != NULL) {
            mmpm_scratch_free(aliases);
        }
        if (offsets != NULL) {
            mmpm_scratch_free(offsets);
        }
        return MMPM_MALLOC_FAIL;
    }

    if (scratch->aliases != NULL) {
        (void)memcpy(aliases, scratch->aliases, scratch->capture_count * sizeof(uint32_t));
        mmpm_scratch_free(scratch->aliases);
    }
    if (scratch->offsets != NULL) {
        (void)memcpy(offsets, scratch->offsets, scratch->capture_count * sizeof(uint64_t));
        mmpm_scratch_free(scratch->offsets);
    }

    scratch->aliases = aliases;
    scratch->offsets = offsets;
    scratch->capture_capacity = new_capacity;
    return MMPM_SUCCESS;
}

static int mmpm_scratch_capture_alias(uint32_t alias, uint64_t end_offset, void *ctx)
{
    mmpm_scratch_t *scratch = (mmpm_scratch_t *)ctx;
    int ret;

    if (scratch == NULL) {
        return MMPM_INVALID_PARA;
    }

    ret = mmpm_scratch_capture_reserve(scratch);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    scratch->aliases[scratch->capture_count] = alias;
    scratch->offsets[scratch->capture_count] = end_offset;
    scratch->capture_count++;
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_scan_database(const mmpm_database_t *db,
                                  const uint8_t *data,
                                  uint32_t len,
                                  mmpm_scratch_t *scratch,
                                  uint32_t *ids,
                                  uint32_t *id_count,
                                  uint32_t id_capacity)
{
    mmpm_handler_t scan_handler = {0};
    uint32_t node_index = 0;
    int scan_ret;
    int ret;

    if (db == NULL || data == NULL || scratch == NULL || scratch->db != db ||
        id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }

    if (db->type != MMPM_MATCHER_TYPE_AC ||
        db->mmpm_matcher == NULL || !db->is_built ||
        db->mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

    *id_count = 0;
    mmpm_scratch_clear_state(scratch);
    scratch->capture_count = 0;
    scan_ret = ac_automaton_match_state_cb((ACAutomaton *)db->mmpm_matcher,
                                           data, len, &node_index,
                                           mmpm_scratch_capture_alias,
                                           scratch);
    if (scan_ret != MMPM_SUCCESS && scan_ret != MMPM_FIND_FAIL) {
        return scan_ret;
    }

    mmpm_database_to_handler(db, &scan_handler);
    ret = mmpm_compile_eval_stream_matches(&scan_handler,
                                           scratch->aliases,
                                           scratch->offsets,
                                           scratch->capture_count,
                                           scratch->logical_vector,
                                           scratch->logical_count,
                                           scratch->combination_vector,
                                           scratch->combination_count,
                                           scratch->exhaustion_vector,
                                           scratch->exhaustion_count,
                                           0,
                                           len,
                                           true,
                                           true,
                                           ids,
                                           id_count,
                                           id_capacity);

    return ret;
}

int mmpm_scan_handler_database(mmpm_handler_t *handler,
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
        handler->mmpm_matcher == NULL || !handler->is_built) {
        return MMPM_INVALID_PARA;
    }

    *id_count = 0;
    if (handler->mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }

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
