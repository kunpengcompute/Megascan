/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <string.h>

#include "mmpm_database.h"
#include "mmpm_ac.h"
#include "mmpm_allocator.h"
#include "mmpm_database_internal.h"
#include "mmpm_stream_internal.h"
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
    mmpm_compile_runtime_workspace_t workspace;
};

static void mmpm_scratch_workspace_destroy(mmpm_compile_runtime_workspace_t *workspace)
{
    if (workspace == NULL) {
        return;
    }

    mmpm_scratch_free(workspace->final_logical_vector);
    mmpm_scratch_free(workspace->active_ckey_vector);
    mmpm_scratch_free(workspace->active_ckeys);
    mmpm_scratch_free(workspace->result_ids);
    mmpm_scratch_free(workspace->new_combination_indexes);
    mmpm_scratch_free(workspace->new_exhaustion_indexes);
    (void)memset(workspace, 0, sizeof(*workspace));
}

static int mmpm_scratch_workspace_init(const mmpm_compile_context_t *ctx,
                                       mmpm_compile_runtime_workspace_t *workspace)
{
    uint32_t logical_bytes;
    uint32_t active_bytes;

    if (workspace == NULL) {
        return MMPM_INVALID_PARA;
    }
    if (ctx == NULL) {
        return MMPM_SUCCESS;
    }
    if (ctx->expr_count > UINT32_MAX - ctx->comb_count) {
        return MMPM_INVALID_PARA;
    }

    logical_bytes = mmpm_compile_logical_byte_count(ctx->logical_count);
    active_bytes = mmpm_compile_logical_byte_count(ctx->comb_count);
    workspace->active_ckey_capacity = ctx->comb_count;
    workspace->result_capacity = ctx->expr_count + ctx->comb_count;
    workspace->new_combination_capacity = ctx->comb_count;
    workspace->new_exhaustion_capacity = ctx->ekey_count;

    if (logical_bytes != 0) {
        workspace->final_logical_vector = (uint8_t *)mmpm_scratch_calloc(
            logical_bytes, sizeof(uint8_t));
    }
    if (active_bytes != 0) {
        workspace->active_ckey_vector = (uint8_t *)mmpm_scratch_calloc(
            active_bytes, sizeof(uint8_t));
    }
    if (ctx->comb_count != 0) {
        workspace->active_ckeys = (uint32_t *)mmpm_scratch_calloc(
            ctx->comb_count, sizeof(uint32_t));
        workspace->new_combination_indexes = (uint32_t *)mmpm_scratch_calloc(
            ctx->comb_count, sizeof(uint32_t));
    }
    if (workspace->result_capacity != 0) {
        workspace->result_ids = (uint32_t *)mmpm_scratch_calloc(
            workspace->result_capacity, sizeof(uint32_t));
    }
    if (ctx->ekey_count != 0) {
        workspace->new_exhaustion_indexes = (uint32_t *)mmpm_scratch_calloc(
            ctx->ekey_count, sizeof(uint32_t));
    }

    if ((logical_bytes != 0 && workspace->final_logical_vector == NULL) ||
        (active_bytes != 0 && workspace->active_ckey_vector == NULL) ||
        (ctx->comb_count != 0 &&
         (workspace->active_ckeys == NULL ||
          workspace->new_combination_indexes == NULL)) ||
        (workspace->result_capacity != 0 && workspace->result_ids == NULL) ||
        (ctx->ekey_count != 0 && workspace->new_exhaustion_indexes == NULL)) {
        mmpm_scratch_workspace_destroy(workspace);
        return MMPM_MALLOC_FAIL;
    }

    return MMPM_SUCCESS;
}

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

    ret = mmpm_scratch_workspace_init(
        (const mmpm_compile_context_t *)scratch_handler.mmpm_compile_ctx,
        &new_scratch->workspace);
    if (ret != MMPM_SUCCESS) {
        mmpm_compile_scratch_state_destroy(new_scratch->logical_vector,
                                           new_scratch->combination_vector,
                                           new_scratch->exhaustion_vector);
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
    mmpm_scratch_workspace_destroy(&scratch->workspace);
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
    ret = mmpm_compile_eval_stream_matches_with_workspace(
        &scan_handler,
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
        id_capacity,
        &scratch->workspace);

    return ret;
}

static int mmpm_scratch_load_stream_state(const ms_stream_t *stream,
                                          mmpm_scratch_t *scratch)
{
    uint32_t bytes;

    if (stream->logical_count != scratch->logical_count ||
        stream->combination_count != scratch->combination_count ||
        stream->exhaustion_count != scratch->exhaustion_count) {
        return MMPM_INVALID_PARA;
    }

    bytes = mmpm_compile_logical_byte_count(stream->logical_count);
    if (bytes != 0) {
        (void)memcpy(scratch->logical_vector, stream->logical_vector, bytes);
    }
    bytes = mmpm_compile_logical_byte_count(stream->combination_count);
    if (bytes != 0) {
        (void)memcpy(scratch->combination_vector, stream->combination_vector, bytes);
    }
    bytes = mmpm_compile_logical_byte_count(stream->exhaustion_count);
    if (bytes != 0) {
        (void)memcpy(scratch->exhaustion_vector, stream->exhaustion_vector, bytes);
    }
    return MMPM_SUCCESS;
}

static void mmpm_scratch_commit_stream_state(ms_stream_t *stream,
                                             const mmpm_scratch_t *scratch)
{
    uint32_t bytes;

    bytes = mmpm_compile_logical_byte_count(stream->logical_count);
    if (bytes != 0) {
        (void)memcpy(stream->logical_vector, scratch->logical_vector, bytes);
    }
    bytes = mmpm_compile_logical_byte_count(stream->combination_count);
    if (bytes != 0) {
        (void)memcpy(stream->combination_vector, scratch->combination_vector, bytes);
    }
    bytes = mmpm_compile_logical_byte_count(stream->exhaustion_count);
    if (bytes != 0) {
        (void)memcpy(stream->exhaustion_vector, scratch->exhaustion_vector, bytes);
    }
}

static int mmpm_validate_database_stream_scratch(const ms_stream_t *stream,
                                                 const mmpm_scratch_t *scratch)
{
    if (stream == NULL || scratch == NULL || stream->database == NULL ||
        scratch->db != stream->database || stream->automaton == NULL ||
        stream->handler_view.mmpm_compile_ctx == NULL) {
        return MMPM_INVALID_PARA;
    }
    if (stream->generation != stream->automaton->generation) {
        return MMPM_STREAM_STALE;
    }
    return MMPM_SUCCESS;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_scan_stream(ms_stream_t *stream,
                                const uint8_t *data,
                                uint32_t len,
                                mmpm_scratch_t *scratch,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity)
{
    uint32_t next_node_index;
    int scan_ret;
    int ret;

    if (data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }
    ret = mmpm_validate_database_stream_scratch(stream, scratch);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }
    ret = mmpm_scratch_load_stream_state(stream, scratch);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    *id_count = 0;
    scratch->capture_count = 0;
    next_node_index = stream->node_index;
    scan_ret = ac_automaton_match_state_cb(stream->automaton, data, len,
                                           &next_node_index,
                                           mmpm_scratch_capture_alias,
                                           scratch);
    if (scan_ret != MMPM_SUCCESS && scan_ret != MMPM_FIND_FAIL) {
        return scan_ret;
    }

    ret = mmpm_compile_eval_stream_matches_with_workspace(
        &stream->handler_view,
        scratch->aliases,
        scratch->offsets,
        scratch->capture_count,
        scratch->logical_vector,
        scratch->logical_count,
        scratch->combination_vector,
        scratch->combination_count,
        scratch->exhaustion_vector,
        scratch->exhaustion_count,
        stream->offset,
        stream->offset + len,
        false,
        false,
        ids,
        id_count,
        id_capacity,
        &scratch->workspace);
    if (ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) {
        stream->node_index = next_node_index;
        stream->offset += len;
        mmpm_scratch_commit_stream_state(stream, scratch);
    }
    return ret;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_flush_stream(ms_stream_t *stream,
                                 mmpm_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity)
{
    int ret;

    if (id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }
    ret = mmpm_validate_database_stream_scratch(stream, scratch);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }
    ret = mmpm_scratch_load_stream_state(stream, scratch);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    *id_count = 0;
    ret = mmpm_compile_eval_stream_matches_with_workspace(
        &stream->handler_view,
        NULL,
        NULL,
        0,
        scratch->logical_vector,
        scratch->logical_count,
        scratch->combination_vector,
        scratch->combination_count,
        scratch->exhaustion_vector,
        scratch->exhaustion_count,
        stream->offset,
        stream->offset,
        true,
        false,
        ids,
        id_count,
        id_capacity,
        &scratch->workspace);
    if (ret == MMPM_SUCCESS || ret == MMPM_FIND_FAIL) {
        mmpm_scratch_commit_stream_state(stream, scratch);
    }
    return ret;
}

__attribute__((visibility("default")))
int MMPM_CDECL mmpm_close_stream(ms_stream_t *stream,
                                 mmpm_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity)
{
    int close_ret;
    int ret;

    if (id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MMPM_INVALID_PARA;
    }
    ret = mmpm_validate_database_stream_scratch(stream, scratch);
    if (ret != MMPM_SUCCESS) {
        return ret;
    }

    ret = mmpm_flush_stream(stream, scratch, ids, id_count, id_capacity);
    close_ret = mmpm_close_handler_stream(stream);
    if (close_ret != MMPM_SUCCESS) {
        return close_ret;
    }
    if (ret == MMPM_FIND_FAIL) {
        return MMPM_SUCCESS;
    }
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
