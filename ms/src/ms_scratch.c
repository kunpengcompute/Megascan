/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <string.h>

#include "ms_database.h"
#include "ms_ac.h"
#include "ms_allocator.h"
#include "ms_runtime.h"
#include "ms_database_internal.h"
#include "ms_stream_internal.h"
#include "expr/ms_expr_internal.h"

struct ms_scratch {
    const ms_database_t *db;
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
    ms_compile_runtime_workspace_t workspace;
};

static void ms_scratch_workspace_destroy(ms_compile_runtime_workspace_t *workspace)
{
    if (workspace == NULL) {
        return;
    }

    ms_scratch_free(workspace->final_logical_vector);
    ms_scratch_free(workspace->active_ckey_vector);
    ms_scratch_free(workspace->active_ckeys);
    ms_scratch_free(workspace->result_ids);
    ms_scratch_free(workspace->new_combination_indexes);
    ms_scratch_free(workspace->new_exhaustion_indexes);
    (void)memset(workspace, 0, sizeof(*workspace));
}

static int ms_scratch_workspace_init(const ms_compile_context_t *ctx,
                                       ms_compile_runtime_workspace_t *workspace)
{
    uint32_t logical_bytes;
    uint32_t active_bytes;

    if (workspace == NULL) {
        return MS_INVALID_PARA;
    }
    if (ctx == NULL) {
        return MS_SUCCESS;
    }
    if (ctx->expr_count > UINT32_MAX - ctx->comb_count) {
        return MS_INVALID_PARA;
    }

    logical_bytes = ms_compile_logical_byte_count(ctx->logical_count);
    active_bytes = ms_compile_logical_byte_count(ctx->comb_count);
    workspace->active_ckey_capacity = ctx->comb_count;
    workspace->result_capacity = ctx->expr_count + ctx->comb_count;
    workspace->new_combination_capacity = ctx->comb_count;
    workspace->new_exhaustion_capacity = ctx->ekey_count;

    if (logical_bytes != 0) {
        workspace->final_logical_vector = (uint8_t *)ms_scratch_calloc(
            logical_bytes, sizeof(uint8_t));
    }
    if (active_bytes != 0) {
        workspace->active_ckey_vector = (uint8_t *)ms_scratch_calloc(
            active_bytes, sizeof(uint8_t));
    }
    if (ctx->comb_count != 0) {
        workspace->active_ckeys = (uint32_t *)ms_scratch_calloc(
            ctx->comb_count, sizeof(uint32_t));
        workspace->new_combination_indexes = (uint32_t *)ms_scratch_calloc(
            ctx->comb_count, sizeof(uint32_t));
    }
    if (workspace->result_capacity != 0) {
        workspace->result_ids = (uint32_t *)ms_scratch_calloc(
            workspace->result_capacity, sizeof(uint32_t));
    }
    if (ctx->ekey_count != 0) {
        workspace->new_exhaustion_indexes = (uint32_t *)ms_scratch_calloc(
            ctx->ekey_count, sizeof(uint32_t));
    }

    if ((logical_bytes != 0 && workspace->final_logical_vector == NULL) ||
        (active_bytes != 0 && workspace->active_ckey_vector == NULL) ||
        (ctx->comb_count != 0 &&
         (workspace->active_ckeys == NULL ||
          workspace->new_combination_indexes == NULL)) ||
        (workspace->result_capacity != 0 && workspace->result_ids == NULL) ||
        (ctx->ekey_count != 0 && workspace->new_exhaustion_indexes == NULL)) {
        ms_scratch_workspace_destroy(workspace);
        return MS_MALLOC_FAIL;
    }

    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int MS_CDECL ms_alloc_scratch(const ms_database_t *db, ms_scratch_t **scratch)
{
    ms_scratch_t *new_scratch;
    int ret;

    if (db == NULL || scratch == NULL || *scratch != NULL) {
        return MS_INVALID_PARA;
    }

    new_scratch = (ms_scratch_t *)ms_scratch_calloc(1, sizeof(ms_scratch_t));
    if (new_scratch == NULL) {
        return MS_MALLOC_FAIL;
    }

    new_scratch->db = db;
    ret = ms_compile_scratch_state_init(db,
                                          &new_scratch->logical_vector,
                                          &new_scratch->logical_count,
                                          &new_scratch->combination_vector,
                                          &new_scratch->combination_count,
                                          &new_scratch->exhaustion_vector,
                                          &new_scratch->exhaustion_count);
    if (ret != MS_SUCCESS) {
        ms_scratch_free(new_scratch);
        return ret;
    }

    ret = ms_scratch_workspace_init(
        (const ms_compile_context_t *)db->ms_compile_ctx,
        &new_scratch->workspace);
    if (ret != MS_SUCCESS) {
        ms_compile_scratch_state_destroy(new_scratch->logical_vector,
                                           new_scratch->combination_vector,
                                           new_scratch->exhaustion_vector);
        ms_scratch_free(new_scratch);
        return ret;
    }

    *scratch = new_scratch;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int MS_CDECL ms_free_scratch(ms_scratch_t *scratch)
{
    if (scratch == NULL) {
        return MS_INVALID_PARA;
    }

    ms_compile_scratch_state_destroy(scratch->logical_vector,
                                       scratch->combination_vector,
                                       scratch->exhaustion_vector);
    if (scratch->aliases != NULL) {
        ms_scratch_free(scratch->aliases);
    }
    if (scratch->offsets != NULL) {
        ms_scratch_free(scratch->offsets);
    }
    ms_scratch_workspace_destroy(&scratch->workspace);
    ms_scratch_free(scratch);
    return MS_SUCCESS;
}

static void ms_scratch_clear_state(ms_scratch_t *scratch)
{
    uint32_t bytes;

    bytes = ms_compile_logical_byte_count(scratch->logical_count);
    if (scratch->logical_vector != NULL && bytes != 0) {
        (void)memset(scratch->logical_vector, 0, bytes);
    }

    bytes = ms_compile_logical_byte_count(scratch->combination_count);
    if (scratch->combination_vector != NULL && bytes != 0) {
        (void)memset(scratch->combination_vector, 0, bytes);
    }

    bytes = ms_compile_logical_byte_count(scratch->exhaustion_count);
    if (scratch->exhaustion_vector != NULL && bytes != 0) {
        (void)memset(scratch->exhaustion_vector, 0, bytes);
    }
}

static int ms_scratch_capture_reserve(ms_scratch_t *scratch)
{
    uint32_t *aliases = NULL;
    uint64_t *offsets = NULL;
    uint32_t new_capacity;

    if (scratch->capture_count < scratch->capture_capacity) {
        return MS_SUCCESS;
    }

    new_capacity = scratch->capture_capacity ? scratch->capture_capacity * 2 : 4;
    aliases = (uint32_t *)ms_scratch_calloc(new_capacity, sizeof(uint32_t));
    offsets = (uint64_t *)ms_scratch_calloc(new_capacity, sizeof(uint64_t));
    if (aliases == NULL || offsets == NULL) {
        if (aliases != NULL) {
            ms_scratch_free(aliases);
        }
        if (offsets != NULL) {
            ms_scratch_free(offsets);
        }
        return MS_MALLOC_FAIL;
    }

    if (scratch->aliases != NULL) {
        (void)memcpy(aliases, scratch->aliases, scratch->capture_count * sizeof(uint32_t));
        ms_scratch_free(scratch->aliases);
    }
    if (scratch->offsets != NULL) {
        (void)memcpy(offsets, scratch->offsets, scratch->capture_count * sizeof(uint64_t));
        ms_scratch_free(scratch->offsets);
    }

    scratch->aliases = aliases;
    scratch->offsets = offsets;
    scratch->capture_capacity = new_capacity;
    return MS_SUCCESS;
}

static int ms_scratch_capture_alias(uint32_t alias, uint64_t end_offset, void *ctx)
{
    ms_scratch_t *scratch = (ms_scratch_t *)ctx;
    int ret;

    if (scratch == NULL) {
        return MS_INVALID_PARA;
    }

    ret = ms_scratch_capture_reserve(scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    scratch->aliases[scratch->capture_count] = alias;
    scratch->offsets[scratch->capture_count] = end_offset;
    scratch->capture_count++;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int MS_CDECL ms_scan_database(const ms_database_t *db,
                                  const uint8_t *data,
                                  uint32_t len,
                                  ms_scratch_t *scratch,
                                  uint32_t *ids,
                                  uint32_t *id_count,
                                  uint32_t id_capacity)
{
    uint32_t node_index = 0;
    int scan_ret;
    int ret;

    if (db == NULL || data == NULL || scratch == NULL || scratch->db != db ||
        id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }

    if (db->type != MS_MATCHER_TYPE_AC ||
        db->ms_matcher == NULL || !db->is_built ||
        db->ms_compile_ctx == NULL) {
        return MS_INVALID_PARA;
    }

    *id_count = 0;
    ms_scratch_clear_state(scratch);
    scratch->capture_count = 0;
    scan_ret = ac_automaton_match_state_cb((ACAutomaton *)db->ms_matcher,
                                           data, len, &node_index,
                                           ms_scratch_capture_alias,
                                           scratch);
    if (scan_ret != MS_SUCCESS && scan_ret != MS_FIND_FAIL) {
        return scan_ret;
    }

    ret = ms_compile_eval_stream_matches_with_workspace(
        db,
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

static int ms_scratch_load_stream_state(const ms_stream_t *stream,
                                          ms_scratch_t *scratch)
{
    uint32_t bytes;

    if (stream->logical_count != scratch->logical_count ||
        stream->combination_count != scratch->combination_count ||
        stream->exhaustion_count != scratch->exhaustion_count) {
        return MS_INVALID_PARA;
    }

    bytes = ms_compile_logical_byte_count(stream->logical_count);
    if (bytes != 0) {
        (void)memcpy(scratch->logical_vector, stream->logical_vector, bytes);
    }
    bytes = ms_compile_logical_byte_count(stream->combination_count);
    if (bytes != 0) {
        (void)memcpy(scratch->combination_vector, stream->combination_vector, bytes);
    }
    bytes = ms_compile_logical_byte_count(stream->exhaustion_count);
    if (bytes != 0) {
        (void)memcpy(scratch->exhaustion_vector, stream->exhaustion_vector, bytes);
    }
    return MS_SUCCESS;
}

static void ms_scratch_commit_stream_state(ms_stream_t *stream,
                                             const ms_scratch_t *scratch)
{
    uint32_t bytes;

    bytes = ms_compile_logical_byte_count(stream->logical_count);
    if (bytes != 0) {
        (void)memcpy(stream->logical_vector, scratch->logical_vector, bytes);
    }
    bytes = ms_compile_logical_byte_count(stream->combination_count);
    if (bytes != 0) {
        (void)memcpy(stream->combination_vector, scratch->combination_vector, bytes);
    }
    bytes = ms_compile_logical_byte_count(stream->exhaustion_count);
    if (bytes != 0) {
        (void)memcpy(stream->exhaustion_vector, scratch->exhaustion_vector, bytes);
    }
}

static int ms_validate_database_stream_scratch(const ms_stream_t *stream,
                                                 const ms_scratch_t *scratch)
{
    const ms_database_t *expected_db;

    if (stream == NULL || scratch == NULL || stream->automaton == NULL ||
        stream->database_view.ms_compile_ctx == NULL) {
        return MS_INVALID_PARA;
    }
    expected_db = (stream->database != NULL) ? stream->database : &stream->database_view;
    if (scratch->db != expected_db) {
        return MS_INVALID_PARA;
    }
    if (stream->generation != stream->automaton->generation) {
        return MS_STREAM_STALE;
    }
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int MS_CDECL ms_scan_stream(ms_stream_t *stream,
                                const uint8_t *data,
                                uint32_t len,
                                ms_scratch_t *scratch,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity)
{
    uint32_t next_node_index;
    int scan_ret;
    int ret;

    if (data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }
    ret = ms_validate_database_stream_scratch(stream, scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    ret = ms_scratch_load_stream_state(stream, scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    *id_count = 0;
    scratch->capture_count = 0;
    next_node_index = stream->node_index;
    scan_ret = ac_automaton_match_state_cb(stream->automaton, data, len,
                                           &next_node_index,
                                           ms_scratch_capture_alias,
                                           scratch);
    if (scan_ret != MS_SUCCESS && scan_ret != MS_FIND_FAIL) {
        return scan_ret;
    }

    ret = ms_compile_eval_stream_matches_with_workspace(
        &stream->database_view,
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
    if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
        stream->node_index = next_node_index;
        stream->offset += len;
        ms_scratch_commit_stream_state(stream, scratch);
    }
    return ret;
}

__attribute__((visibility("default")))
int MS_CDECL ms_flush_stream(ms_stream_t *stream,
                                 ms_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity)
{
    int ret;

    if (id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }
    ret = ms_validate_database_stream_scratch(stream, scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    ret = ms_scratch_load_stream_state(stream, scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    *id_count = 0;
    ret = ms_compile_eval_stream_matches_with_workspace(
        &stream->database_view,
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
    if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
        ms_scratch_commit_stream_state(stream, scratch);
    }
    return ret;
}

__attribute__((visibility("default")))
int MS_CDECL ms_close_stream(ms_stream_t *stream,
                                 ms_scratch_t *scratch,
                                 uint32_t *ids,
                                 uint32_t *id_count,
                                 uint32_t id_capacity)
{
    int close_ret;
    int ret;

    if (id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }
    ret = ms_validate_database_stream_scratch(stream, scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    ret = ms_flush_stream(stream, scratch, ids, id_count, id_capacity);
    close_ret = ms_close_handler_stream(stream);
    if (close_ret != MS_SUCCESS) {
        return close_ret;
    }
    if (ret == MS_FIND_FAIL) {
        return MS_SUCCESS;
    }
    return ret;
}
