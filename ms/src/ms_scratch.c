/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 */
#include <string.h>

#include "ms_database.h"
#include "ms_ac.h"
#include "ms_acdb.h"
#include "ms_allocator.h"
#include "ms_runtime.h"
#include "ms_database_internal.h"
#include "ms_stream_internal.h"
#include "expr/ms_expr_internal.h"

typedef struct {
    uint8_t *marked;
    uint32_t byte_count;
    uint32_t *indexes;
    uint8_t *original_values;
    uint32_t count;
    uint32_t capacity;
} ms_state_change_log_t;

struct ms_scratch {
    const ms_database_t *db;
    uint64_t generation;
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
    uint64_t capture_start_offset;
    bool capture_valid;
    ms_compile_runtime_workspace_t workspace;
    ms_state_change_log_t logical_changes;
    ms_state_change_log_t combination_changes;
    ms_state_change_log_t exhaustion_changes;
    ms_state_change_log_t result_seen_changes;
    bool block_state_dirty;
};

static int ms_scratch_track_block_results(const ms_compile_context_t *ctx,
                                          ms_scratch_t *scratch);
static int ms_scratch_track_result_key(ms_scratch_t *scratch, uint32_t key);

static void ms_state_change_log_destroy(ms_state_change_log_t *log)
{
    if (log == NULL) {
        return;
    }
    ms_scratch_free(log->marked);
    ms_scratch_free(log->indexes);
    ms_scratch_free(log->original_values);
    (void)memset(log, 0, sizeof(*log));
}

static int ms_state_change_log_reserve(ms_state_change_log_t *log)
{
    uint32_t new_capacity;
    uint32_t *new_indexes;
    uint8_t *new_values;

    if (log->count < log->capacity) {
        return MS_SUCCESS;
    }
    new_capacity = log->capacity == 0 ? 64u : log->capacity * 2u;
    if (new_capacity < log->capacity || new_capacity > log->byte_count) {
        new_capacity = log->byte_count;
    }
    if (new_capacity <= log->capacity) {
        return MS_INVALID_PARA;
    }

    new_indexes = (uint32_t *)ms_scratch_calloc(new_capacity,
                                                 sizeof(uint32_t));
    new_values = (uint8_t *)ms_scratch_calloc(new_capacity, sizeof(uint8_t));
    if (new_indexes == NULL || new_values == NULL) {
        ms_scratch_free(new_indexes);
        ms_scratch_free(new_values);
        return MS_MALLOC_FAIL;
    }
    if (log->count != 0) {
        (void)memcpy(new_indexes, log->indexes,
                     log->count * sizeof(uint32_t));
        (void)memcpy(new_values, log->original_values,
                     log->count * sizeof(uint8_t));
    }
    ms_scratch_free(log->indexes);
    ms_scratch_free(log->original_values);
    log->indexes = new_indexes;
    log->original_values = new_values;
    log->capacity = new_capacity;
    return MS_SUCCESS;
}

static int ms_state_change_log_mark(ms_state_change_log_t *log,
                                    uint8_t *vector, uint32_t bit_count,
                                    uint32_t key)
{
    uint32_t byte_count = ms_compile_logical_byte_count(bit_count);
    uint32_t byte_index;
    int ret;

    if (log == NULL || vector == NULL || key >= bit_count) {
        return MS_INVALID_PARA;
    }
    if (log->marked == NULL) {
        log->marked = (uint8_t *)ms_scratch_calloc(byte_count, sizeof(uint8_t));
        if (log->marked == NULL) {
            return MS_MALLOC_FAIL;
        }
        log->byte_count = byte_count;
    } else if (log->byte_count != byte_count) {
        return MS_INVALID_PARA;
    }

    byte_index = key / 8u;
    if (log->marked[byte_index] != 0) {
        return MS_SUCCESS;
    }
    ret = ms_state_change_log_reserve(log);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    log->marked[byte_index] = 1;
    log->indexes[log->count] = byte_index;
    log->original_values[log->count] = vector[byte_index];
    log->count++;
    return MS_SUCCESS;
}

static void ms_state_change_log_finish(ms_state_change_log_t *log,
                                       uint8_t *vector, bool restore)
{
    uint32_t i;

    if (log == NULL) {
        return;
    }
    for (i = 0; i < log->count; i++) {
        uint32_t index = log->indexes[i];

        if (restore && vector != NULL) {
            vector[index] = log->original_values[i];
        }
        log->marked[index] = 0;
    }
    log->count = 0;
}

static void ms_state_change_log_clear_vector(ms_state_change_log_t *log,
                                             uint8_t *vector)
{
    uint32_t i;

    if (log == NULL) {
        return;
    }
    for (i = 0; i < log->count; i++) {
        uint32_t index = log->indexes[i];

        if (vector != NULL) {
            vector[index] = 0;
        }
        log->marked[index] = 0;
    }
    log->count = 0;
}

static void ms_scratch_workspace_destroy(ms_compile_runtime_workspace_t *workspace)
{
    if (workspace == NULL) {
        return;
    }

    ms_scratch_free(workspace->final_logical_vector);
    ms_scratch_free(workspace->active_ckey_vector);
    ms_scratch_free(workspace->active_ckeys);
    ms_scratch_free(workspace->result_ids);
    ms_scratch_free(workspace->result_seen_vector);
    ms_scratch_free(workspace->new_combination_indexes);
    ms_scratch_free(workspace->new_exhaustion_indexes);
    (void)memset(workspace, 0, sizeof(*workspace));
}

static int ms_scratch_workspace_init(const ms_compile_context_t *ctx,
                                       ms_compile_runtime_workspace_t *workspace)
{
    uint32_t logical_bytes;
    uint32_t active_bytes;
    uint32_t result_bytes;

    if (workspace == NULL) {
        return MS_INVALID_PARA;
    }
    if (ctx == NULL) {
        return MS_SUCCESS;
    }
    if (ctx->expr_count > UINT32_MAX - ctx->comb_count ||
        ctx->logical_count > UINT32_MAX - ctx->comb_count) {
        return MS_INVALID_PARA;
    }

    logical_bytes = ms_compile_logical_byte_count(ctx->logical_count);
    active_bytes = ms_compile_logical_byte_count(ctx->comb_count);
    workspace->active_ckey_capacity = ctx->comb_count;
    workspace->result_capacity = ctx->expr_count + ctx->comb_count;
    workspace->result_seen_count = ctx->logical_count + ctx->comb_count;
    workspace->new_combination_capacity = ctx->comb_count;
    workspace->new_exhaustion_capacity = ctx->ekey_count;

    if (ctx->comb_count != 0 && logical_bytes != 0) {
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
    if (workspace->result_seen_count != 0) {
        result_bytes = ms_compile_logical_byte_count(workspace->result_seen_count);
        workspace->result_seen_vector = (uint8_t *)ms_scratch_calloc(
            result_bytes, sizeof(uint8_t));
    }
    if (ctx->ekey_count != 0) {
        workspace->new_exhaustion_indexes = (uint32_t *)ms_scratch_calloc(
            ctx->ekey_count, sizeof(uint32_t));
    }

    if ((ctx->comb_count != 0 && logical_bytes != 0 &&
         workspace->final_logical_vector == NULL) ||
        (active_bytes != 0 && workspace->active_ckey_vector == NULL) ||
        (ctx->comb_count != 0 &&
         (workspace->active_ckeys == NULL ||
          workspace->new_combination_indexes == NULL)) ||
        (workspace->result_capacity != 0 && workspace->result_ids == NULL) ||
        (workspace->result_seen_count != 0 &&
         workspace->result_seen_vector == NULL) ||
        (ctx->ekey_count != 0 && workspace->new_exhaustion_indexes == NULL)) {
        ms_scratch_workspace_destroy(workspace);
        return MS_MALLOC_FAIL;
    }

    return MS_SUCCESS;
}

static uint64_t ms_scratch_database_generation(const ms_database_t *db)
{
    if (db == NULL || db->type != MS_MATCHER_TYPE_AC || db->ms_matcher == NULL) {
        return 0;
    }
    if (db->backend == MS_DATABASE_BACKEND_COMPACT_AC) {
        return ms_acdb_generation((const ms_acdb_t *)db->ms_matcher);
    }
    return ((const ACAutomaton *)db->ms_matcher)->generation;
}

static int ms_scratch_refresh(ms_scratch_t *scratch)
{
    const ms_database_t *db;
    ms_compile_runtime_workspace_t workspace = {0};
    uint8_t *logical_vector = NULL;
    uint8_t *combination_vector = NULL;
    uint8_t *exhaustion_vector = NULL;
    uint32_t logical_count = 0;
    uint32_t combination_count = 0;
    uint32_t exhaustion_count = 0;
    uint64_t generation;
    int ret;

    if (scratch == NULL || scratch->db == NULL) {
        return MS_INVALID_PARA;
    }
    db = scratch->db;
    generation = ms_scratch_database_generation(db);
    if (scratch->generation == generation) {
        return MS_SUCCESS;
    }

    ret = ms_compile_scratch_state_init(db, &logical_vector, &logical_count,
                                         &combination_vector, &combination_count,
                                         &exhaustion_vector, &exhaustion_count);
    if (ret == MS_SUCCESS) {
        ret = ms_scratch_workspace_init(
            (const ms_compile_context_t *)db->ms_compile_ctx, &workspace);
    }
    if (ret != MS_SUCCESS) {
        ms_compile_scratch_state_destroy(logical_vector, combination_vector,
                                          exhaustion_vector);
        ms_scratch_workspace_destroy(&workspace);
        return ret;
    }

    ms_compile_scratch_state_destroy(scratch->logical_vector,
                                      scratch->combination_vector,
                                      scratch->exhaustion_vector);
    ms_scratch_workspace_destroy(&scratch->workspace);
    ms_state_change_log_destroy(&scratch->logical_changes);
    ms_state_change_log_destroy(&scratch->combination_changes);
    ms_state_change_log_destroy(&scratch->exhaustion_changes);
    ms_state_change_log_destroy(&scratch->result_seen_changes);
    scratch->logical_vector = logical_vector;
    scratch->logical_count = logical_count;
    scratch->combination_vector = combination_vector;
    scratch->combination_count = combination_count;
    scratch->exhaustion_vector = exhaustion_vector;
    scratch->exhaustion_count = exhaustion_count;
    scratch->workspace = workspace;
    scratch->capture_count = 0;
    scratch->block_state_dirty = false;
    scratch->generation = generation;
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_alloc_scratch(const ms_database_t *db, ms_scratch_t **scratch)
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
    new_scratch->generation = ms_scratch_database_generation(db);
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
int ms_free_scratch(ms_scratch_t *scratch)
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
    ms_state_change_log_destroy(&scratch->logical_changes);
    ms_state_change_log_destroy(&scratch->combination_changes);
    ms_state_change_log_destroy(&scratch->exhaustion_changes);
    ms_state_change_log_destroy(&scratch->result_seen_changes);
    ms_scratch_workspace_destroy(&scratch->workspace);
    ms_scratch_free(scratch);
    return MS_SUCCESS;
}

static void ms_scratch_clear_key(uint8_t *vector, uint32_t count,
                                 uint32_t key)
{
    if (vector != NULL && key < count) {
        vector[key / 8u] &= (uint8_t)~(1u << (key % 8u));
    }
}

static void ms_scratch_clear_combination_state(
    ms_scratch_t *scratch, const ms_compile_context_t *ctx, uint32_t ckey)
{
    const ms_compile_comb_t *comb;
    uint32_t i;

    if (ckey >= ctx->comb_count) {
        return;
    }
    comb = &ctx->combs[ckey];
    for (i = 0; i < comb->tree.op_count; i++) {
        uint32_t lkey = comb->tree.ops[i].id;

        ms_scratch_clear_key(scratch->logical_vector,
                             scratch->logical_count, lkey);
        ms_scratch_clear_key(scratch->workspace.final_logical_vector,
                             scratch->logical_count, lkey);
    }
    ms_scratch_clear_key(scratch->combination_vector,
                         scratch->combination_count, ckey);
    if (comb->ekey != UINT32_MAX) {
        ms_scratch_clear_key(scratch->exhaustion_vector,
                             scratch->exhaustion_count, comb->ekey);
    }
}

static void ms_scratch_clear_compact_ac_state(
    ms_scratch_t *scratch, const ms_compile_context_t *ctx)
{
    uint32_t i;

    for (i = 0; i < scratch->capture_count; i++) {
        uint32_t lkey;
        uint32_t j;

        if (ms_compile_context_alias_to_lkey(ctx, scratch->aliases[i],
                                             &lkey) != MS_SUCCESS ||
            lkey >= ctx->logical_count) {
            continue;
        }
        ms_scratch_clear_key(scratch->logical_vector,
                             scratch->logical_count, lkey);
        ms_scratch_clear_key(scratch->workspace.final_logical_vector,
                             scratch->logical_count, lkey);
        for (j = 0; j < ctx->lkey_to_ckey_counts[lkey]; j++) {
            ms_scratch_clear_combination_state(
                scratch, ctx, ctx->lkey_to_ckeys[lkey][j]);
        }
    }
    for (i = 0; i < ctx->final_ckey_count; i++) {
        ms_scratch_clear_combination_state(scratch, ctx,
                                           ctx->final_ckeys[i]);
    }
}

static void ms_scratch_clear_state(ms_scratch_t *scratch)
{
    const ms_compile_context_t *ctx =
        (const ms_compile_context_t *)scratch->db->ms_compile_ctx;
    uint32_t bytes;

    ms_state_change_log_clear_vector(
        &scratch->result_seen_changes,
        scratch->workspace.result_seen_vector);

    if (ctx != NULL && ctx->ac_aliases_are_lkeys) {
        ms_scratch_clear_compact_ac_state(scratch, ctx);
        return;
    } else {
        bytes = ms_compile_logical_byte_count(scratch->logical_count);
        if (scratch->logical_vector != NULL && bytes != 0) {
            (void)memset(scratch->logical_vector, 0, bytes);
        }
        if (scratch->workspace.final_logical_vector != NULL && bytes != 0) {
            (void)memset(scratch->workspace.final_logical_vector, 0, bytes);
        }
        bytes = ms_compile_logical_byte_count(
            scratch->workspace.result_seen_count);
        if (scratch->workspace.result_seen_vector != NULL && bytes != 0) {
            (void)memset(scratch->workspace.result_seen_vector, 0, bytes);
        }
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

static void ms_scratch_begin_scan(ms_scratch_t *scratch)
{
    if (scratch->block_state_dirty) {
        ms_scratch_clear_state(scratch);
    } else {
        ms_state_change_log_clear_vector(
            &scratch->result_seen_changes,
            scratch->workspace.result_seen_vector);
    }
    scratch->block_state_dirty = false;
    scratch->capture_count = 0;
    scratch->capture_valid = false;
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
int ms_get_match_events(const ms_scratch_t *scratch,
                        ms_match_event_t *events,
                        uint32_t *event_count,
                        uint32_t event_capacity)
{
    const ms_compile_context_t *ctx;
    uint32_t required = 0;
    uint32_t i;

    if (scratch == NULL || event_count == NULL || !scratch->capture_valid ||
        (event_capacity != 0 && events == NULL)) {
        return MS_INVALID_PARA;
    }
    ctx = (const ms_compile_context_t *)scratch->db->ms_compile_ctx;
    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }
    for (i = 0; i < scratch->capture_count; i++) {
        const ms_compile_expr_t *expr =
            ms_compile_context_find_expr_for_alias(ctx, scratch->aliases[i]);

        if (expr != NULL && (expr->flags & MS_FLAG_QUIET) == 0) {
            required++;
        }
    }
    *event_count = required;
    if (events == NULL && event_capacity == 0) {
        return MS_SUCCESS;
    }
    if (required > event_capacity) {
        return MS_INVALID_PARA;
    }

    required = 0;
    for (i = 0; i < scratch->capture_count; i++) {
        const ms_compile_expr_t *expr =
            ms_compile_context_find_expr_for_alias(ctx, scratch->aliases[i]);
        uint64_t to;

        if (expr == NULL || (expr->flags & MS_FLAG_QUIET) != 0) {
            continue;
        }
        to = scratch->capture_start_offset + scratch->offsets[i];
        events[required].id = expr->id;
        events[required].from = to >= expr->pattern_len
                                    ? to - expr->pattern_len
                                    : 0;
        events[required].to = to;
        required++;
    }
    return MS_SUCCESS;
}

__attribute__((visibility("default")))
int ms_scan_database(const ms_database_t *db,
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

    ret = ms_scratch_refresh(scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    *id_count = 0;
    ms_scratch_begin_scan(scratch);
    if (db->backend == MS_DATABASE_BACKEND_COMPACT_AC) {
        scan_ret = ms_acdb_match_state_cb((const ms_acdb_t *)db->ms_matcher,
                                          data, len, &node_index,
                                          ms_scratch_capture_alias,
                                          scratch);
    } else {
        scan_ret = ac_automaton_match_state_cb((ACAutomaton *)db->ms_matcher,
                                               data, len, &node_index,
                                               ms_scratch_capture_alias,
                                               scratch);
    }
    if (scan_ret != MS_SUCCESS && scan_ret != MS_FIND_FAIL) {
        return scan_ret;
    }

    ret = ms_scratch_track_block_results(
        (const ms_compile_context_t *)db->ms_compile_ctx, scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }

    scratch->block_state_dirty = true;
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

    if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
        scratch->capture_start_offset = 0;
        scratch->capture_valid = true;
    }

    return ret;
}

static int ms_stream_dirty_bytes_reserve(ms_stream_dirty_bytes_t *dirty)
{
    uint32_t new_capacity;
    uint32_t *new_indexes;

    if (dirty->count < dirty->capacity) {
        return MS_SUCCESS;
    }
    new_capacity = dirty->capacity == 0 ? 64u : dirty->capacity * 2u;
    if (new_capacity < dirty->capacity || new_capacity > dirty->byte_count) {
        new_capacity = dirty->byte_count;
    }
    if (new_capacity <= dirty->capacity) {
        return MS_INVALID_PARA;
    }

    new_indexes = (uint32_t *)ms_stream_calloc(new_capacity,
                                                sizeof(uint32_t));
    if (new_indexes == NULL) {
        return MS_MALLOC_FAIL;
    }
    if (dirty->count != 0) {
        (void)memcpy(new_indexes, dirty->indexes,
                     dirty->count * sizeof(uint32_t));
    }
    ms_stream_free(dirty->indexes);
    dirty->indexes = new_indexes;
    dirty->capacity = new_capacity;
    return MS_SUCCESS;
}

static int ms_stream_dirty_bytes_add(ms_stream_dirty_bytes_t *dirty,
                                     uint32_t byte_count,
                                     uint32_t byte_index)
{
    int ret;

    if (dirty->marked == NULL) {
        dirty->marked = (uint8_t *)ms_stream_calloc(byte_count,
                                                    sizeof(uint8_t));
        if (dirty->marked == NULL) {
            return MS_MALLOC_FAIL;
        }
        dirty->byte_count = byte_count;
    } else if (dirty->byte_count != byte_count) {
        return MS_INVALID_PARA;
    }
    if (byte_index >= byte_count) {
        return MS_INVALID_PARA;
    }
    if (dirty->marked[byte_index] != 0) {
        return MS_SUCCESS;
    }
    ret = ms_stream_dirty_bytes_reserve(dirty);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    dirty->marked[byte_index] = 1;
    dirty->indexes[dirty->count++] = byte_index;
    return MS_SUCCESS;
}

static int ms_stream_dirty_bytes_add_log(ms_stream_dirty_bytes_t *dirty,
                                         uint32_t bit_count,
                                         const ms_state_change_log_t *log)
{
    uint32_t byte_count = ms_compile_logical_byte_count(bit_count);
    uint32_t i;
    int ret;

    for (i = 0; i < log->count; i++) {
        ret = ms_stream_dirty_bytes_add(dirty, byte_count, log->indexes[i]);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }
    return MS_SUCCESS;
}

static int ms_scratch_log_combination_state(ms_scratch_t *scratch,
                                            ms_stream_t *stream,
                                            const ms_compile_comb_t *comb)
{
    uint32_t i;
    int ret;

    if (comb == NULL || comb->ckey >= stream->combination_count) {
        return MS_INVALID_PARA;
    }
    for (i = 0; i < comb->tree.op_count; i++) {
        ret = ms_state_change_log_mark(&scratch->logical_changes,
                                       stream->logical_vector,
                                       stream->logical_count,
                                       comb->tree.ops[i].id);
        if (ret != MS_SUCCESS) {
            return ret;
        }
    }
    ret = ms_state_change_log_mark(&scratch->combination_changes,
                                   stream->combination_vector,
                                   stream->combination_count, comb->ckey);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    if (comb->ekey != UINT32_MAX) {
        ret = ms_state_change_log_mark(&scratch->exhaustion_changes,
                                       stream->exhaustion_vector,
                                       stream->exhaustion_count, comb->ekey);
    }
    return ret;
}

static int ms_scratch_prepare_stream_changes(ms_stream_t *stream,
                                             ms_scratch_t *scratch,
                                             bool final_flush)
{
    const ms_compile_context_t *ctx =
        (const ms_compile_context_t *)stream->database_view.ms_compile_ctx;
    uint32_t i;
    int ret;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }
    if (final_flush) {
        for (i = 0; i < ctx->final_ckey_count; i++) {
            uint32_t ckey = ctx->final_ckeys[i];

            if (ckey >= ctx->comb_count) {
                return MS_INVALID_PARA;
            }
            ret = ms_scratch_track_result_key(scratch,
                                              ctx->logical_count + ckey);
            if (ret == MS_SUCCESS) {
                ret = ms_scratch_log_combination_state(scratch, stream,
                                                       &ctx->combs[ckey]);
            }
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }
        return MS_SUCCESS;
    }

    for (i = 0; i < scratch->capture_count; i++) {
        uint32_t lkey;
        uint32_t j;

        ret = ms_compile_context_alias_to_lkey(ctx, scratch->aliases[i],
                                               &lkey);
        if (ret == MS_FIND_FAIL) {
            continue;
        }
        if (ret != MS_SUCCESS || lkey >= ctx->logical_count) {
            return ret == MS_SUCCESS ? MS_INVALID_PARA : ret;
        }
        ret = ms_scratch_track_result_key(scratch, lkey);
        if (ret != MS_SUCCESS) {
            return ret;
        }
        if (ctx->comb_count == 0) {
            continue;
        }
        ret = ms_state_change_log_mark(&scratch->logical_changes,
                                       stream->logical_vector,
                                       stream->logical_count, lkey);
        if (ret != MS_SUCCESS) {
            return ret;
        }
        for (j = 0; j < ctx->lkey_to_ckey_counts[lkey]; j++) {
            uint32_t ckey = ctx->lkey_to_ckeys[lkey][j];

            if (ckey >= ctx->comb_count) {
                return MS_INVALID_PARA;
            }
            ret = ms_scratch_track_result_key(scratch,
                                              ctx->logical_count + ckey);
            if (ret == MS_SUCCESS) {
                ret = ms_scratch_log_combination_state(scratch, stream,
                                                       &ctx->combs[ckey]);
            }
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }
    }
    return MS_SUCCESS;
}

static void ms_scratch_finish_stream_changes(ms_stream_t *stream,
                                             ms_scratch_t *scratch,
                                             bool restore)
{
    ms_state_change_log_finish(&scratch->logical_changes,
                               stream->logical_vector, restore);
    ms_state_change_log_finish(&scratch->combination_changes,
                               stream->combination_vector, restore);
    ms_state_change_log_finish(&scratch->exhaustion_changes,
                               stream->exhaustion_vector, restore);
}

static int ms_scratch_commit_stream_changes(ms_stream_t *stream,
                                            ms_scratch_t *scratch)
{
    int ret;

    ret = ms_stream_dirty_bytes_add_log(&stream->logical_dirty,
                                        stream->logical_count,
                                        &scratch->logical_changes);
    if (ret == MS_SUCCESS) {
        ret = ms_stream_dirty_bytes_add_log(&stream->combination_dirty,
                                            stream->combination_count,
                                            &scratch->combination_changes);
    }
    if (ret == MS_SUCCESS) {
        ret = ms_stream_dirty_bytes_add_log(&stream->exhaustion_dirty,
                                            stream->exhaustion_count,
                                            &scratch->exhaustion_changes);
    }
    ms_scratch_finish_stream_changes(stream, scratch, ret != MS_SUCCESS);
    return ret;
}

static int ms_scratch_track_result_key(ms_scratch_t *scratch, uint32_t key)
{
    return ms_state_change_log_mark(
        &scratch->result_seen_changes,
        scratch->workspace.result_seen_vector,
        scratch->workspace.result_seen_count, key);
}

static int ms_scratch_track_block_results(const ms_compile_context_t *ctx,
                                          ms_scratch_t *scratch)
{
    uint32_t i;
    int ret;

    if (ctx == NULL) {
        return MS_INVALID_PARA;
    }
    for (i = 0; i < scratch->capture_count; i++) {
        uint32_t lkey;
        uint32_t j;

        if (ms_compile_context_alias_to_lkey(ctx, scratch->aliases[i],
                                             &lkey) != MS_SUCCESS ||
            lkey >= ctx->logical_count) {
            continue;
        }
        ret = ms_scratch_track_result_key(scratch, lkey);
        if (ret != MS_SUCCESS) {
            return ret;
        }
        for (j = 0; j < ctx->lkey_to_ckey_counts[lkey]; j++) {
            uint32_t ckey = ctx->lkey_to_ckeys[lkey][j];

            if (ckey < ctx->comb_count) {
                ret = ms_scratch_track_result_key(
                    scratch, ctx->logical_count + ckey);
                if (ret != MS_SUCCESS) {
                    return ret;
                }
            }
        }
    }
    for (i = 0; i < ctx->final_ckey_count; i++) {
        if (ctx->final_ckeys[i] < ctx->comb_count) {
            ret = ms_scratch_track_result_key(
                scratch, ctx->logical_count + ctx->final_ckeys[i]);
            if (ret != MS_SUCCESS) {
                return ret;
            }
        }
    }
    return MS_SUCCESS;
}

static int ms_validate_database_stream_scratch(const ms_stream_t *stream,
                                                 ms_scratch_t *scratch)
{
    const ms_database_t *expected_db;

    if (stream == NULL || scratch == NULL ||
        (stream->automaton == NULL && stream->acdb == NULL) ||
        stream->database_view.ms_compile_ctx == NULL) {
        return MS_INVALID_PARA;
    }
    expected_db = (stream->database != NULL) ? stream->database : &stream->database_view;
    if (scratch->db != expected_db) {
        return MS_INVALID_PARA;
    }
    if ((stream->automaton != NULL &&
         stream->generation != stream->automaton->generation) ||
        (stream->acdb != NULL &&
         stream->generation != ms_acdb_generation(stream->acdb))) {
        return MS_STREAM_STALE;
    }
    return ms_scratch_refresh(scratch);
}

__attribute__((visibility("default")))
int ms_scan_stream(ms_stream_t *stream,
                                const uint8_t *data,
                                uint32_t len,
                                ms_scratch_t *scratch,
                                uint32_t *ids,
                                uint32_t *id_count,
                                uint32_t id_capacity)
{
    uint32_t next_node_index;
    uint64_t scan_start_offset;
    int scan_ret;
    int ret;

    if (data == NULL || id_count == NULL || (id_capacity != 0 && ids == NULL)) {
        return MS_INVALID_PARA;
    }
    ret = ms_validate_database_stream_scratch(stream, scratch);
    if (ret != MS_SUCCESS) {
        return ret;
    }
    *id_count = 0;
    ms_scratch_begin_scan(scratch);
    scan_start_offset = stream->offset;
    next_node_index = stream->node_index;
    if (stream->acdb != NULL) {
        scan_ret = ms_acdb_match_state_cb(stream->acdb, data, len,
                                          &next_node_index,
                                          ms_scratch_capture_alias,
                                          scratch);
    } else {
        scan_ret = ac_automaton_match_state_cb(stream->automaton, data, len,
                                               &next_node_index,
                                               ms_scratch_capture_alias,
                                               scratch);
    }
    if (scan_ret != MS_SUCCESS && scan_ret != MS_FIND_FAIL) {
        return scan_ret;
    }

    ret = ms_scratch_prepare_stream_changes(stream, scratch, false);
    if (ret != MS_SUCCESS) {
        ms_scratch_finish_stream_changes(stream, scratch, false);
        return ret;
    }

    ret = ms_compile_eval_stream_matches_with_workspace(
        &stream->database_view,
        scratch->aliases,
        scratch->offsets,
        scratch->capture_count,
        stream->logical_vector,
        stream->logical_count,
        stream->combination_vector,
        stream->combination_count,
        stream->exhaustion_vector,
        stream->exhaustion_count,
        stream->offset,
        stream->offset + len,
        false,
        false,
        ids,
        id_count,
        id_capacity,
        &scratch->workspace);
    if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
        int eval_ret = ret;
        int commit_ret = ms_scratch_commit_stream_changes(stream, scratch);

        if (commit_ret != MS_SUCCESS) {
            return commit_ret;
        }
        stream->node_index = next_node_index;
        stream->offset += len;
        scratch->capture_start_offset = scan_start_offset;
        scratch->capture_valid = true;
        ret = eval_ret;
    } else {
        ms_scratch_finish_stream_changes(stream, scratch, true);
    }
    return ret;
}

__attribute__((visibility("default")))
int ms_flush_stream(ms_stream_t *stream,
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
    *id_count = 0;
    ms_scratch_begin_scan(scratch);
    ret = ms_scratch_prepare_stream_changes(stream, scratch, true);
    if (ret != MS_SUCCESS) {
        ms_scratch_finish_stream_changes(stream, scratch, false);
        return ret;
    }
    ret = ms_compile_eval_stream_matches_with_workspace(
        &stream->database_view,
        NULL,
        NULL,
        0,
        stream->logical_vector,
        stream->logical_count,
        stream->combination_vector,
        stream->combination_count,
        stream->exhaustion_vector,
        stream->exhaustion_count,
        stream->offset,
        stream->offset,
        true,
        false,
        ids,
        id_count,
        id_capacity,
        &scratch->workspace);
    if (ret == MS_SUCCESS || ret == MS_FIND_FAIL) {
        int eval_ret = ret;
        int commit_ret = ms_scratch_commit_stream_changes(stream, scratch);

        if (commit_ret != MS_SUCCESS) {
            return commit_ret;
        }
        scratch->capture_start_offset = stream->offset;
        scratch->capture_valid = true;
        ret = eval_ret;
    } else {
        ms_scratch_finish_stream_changes(stream, scratch, true);
    }
    return ret;
}

__attribute__((visibility("default")))
int ms_close_stream(ms_stream_t *stream,
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
